#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdalign.h>

#include "index/b_tree.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

/**
 * Implementação de uma árvore B genérica, flexível quanto ao tamanho
 * da página: basta alterar o valor da macro `PAGE_SIZE` para usar um
 * tamanho diferente de página de disco.
 */

#define PAGE_SIZE 44

#define B_STATUS_INCONSISTENT '0'
#define B_STATUS_CONSISTENT   '1'

#define SUBNODE_FIELDS(X)  \
    X(uint32_t, left)      \
    X(uint32_t, key)       \
    X(uint64_t, offset)    \
    X(uint32_t, right)

#define X(T, name) T name;    

/**
 * Um "subnó" é composto por uma chave `key`, o offset do registro
 * correspondente no arquivo de dados, o filho imediatamente anterior
 * a `key` e o filho imediatamente posterior a `key`, apelidados de
 * `left` e `right`. Na página de disco da árvore B, "subnós" não
 * existem: o campo `right` de um subnó e o campo `left` do próximo
 * subnó são, na verdade, o mesmo campo. O subnó é apenas uma abstração
 * utilizada para simplificar a implementação.
 *
 * Subnós são lidos/escritos um por vez, para busca/inserção apenas.
 * Outras operações, como deslocamento e distribuição uniforme das
 * chaves, são realizadas diretamente na página (em memória primária).
 */
typedef struct {
    SUBNODE_FIELDS(X)
} b_tree_subnode_t;

#undef X

/**
 * Tamanho do campo `left` de `b_tree_subnode_t`.
 *
 * O valor dessa macro, bem como o da macro `SUBNODE_SKIP`, foi deixado "hardcoded",
 * em vez de ser definido usando `sizeof` dos campos, uma vez que essa macro é usada
 * no contexto de diretivas de pré-processamento para compilação condicional.
 * `static_assert` é usado para garantir que esse valor está correto.
 */
#define SIZE_LEFT    4 /* sizeof left */

/**
 * Quantidade de bytes a serem pulados para ir de um "subnó" para o
 * próximo em uma página. Não inclui o campo `left` devido à sobreposição
 * entre os campos `left` e `right` mencionada acima.
 */
#define SUBNODE_SKIP (4 /* sizeof key */ + 8 /* sizeof offset */ + 4 /* sizeof right */)

/** Quantidade de bytes ocupados por metadados nas páginas da árvore */
#define PAGE_META_SIZE (4 /* sizeof type */ + 4 /* sizeof len */)

/** Quantidade de chaves que podem ser armazenadas em um nó/uma página da árvore */
#define N_KEYS ((PAGE_SIZE - PAGE_META_SIZE - SIZE_LEFT) / SUBNODE_SKIP)

/** Ordem da árvore */
#define TREE_ORDER (N_KEYS + 1)

/** Verdadeiro se houver espaço no final da página da árvore, que deverá ser preenchido com '$' */
#define TREE_PAGE_NEEDS_PADDING N_KEYS * SUBNODE_SKIP + SIZE_LEFT < PAGE_SIZE - PAGE_META_SIZE

/** Guarda uma página (nó) da árvore B */
typedef struct {
    alignas(uint32_t) char data[PAGE_SIZE];
} b_tree_page_t;

/** Para as macros a seguir, `p` deve ser um `b_tree_page_t *`. */

/** Ponteiro para o tipo do nó */
#define NODE_TYPE_P(p) ((uint32_t *)&(p)->data[0])
/** Ponteiro para o tamanho do nó (quantidade de chaves armazenadas) */
#define NODE_LEN_P(p)  ((uint32_t *)&(p)->data[4])
/** Ponteiro para os dados do nó */
#define NODE_DATA_P(p) &(p)->data[8]

enum {
    /**
     * Possui precedência sobre `NODE_TYPE_ROOT`, ou seja,
     * o tipo de um nó que é simultaneamente raiz e folha
     * é `NODE_TYPE_LEAF`.
     */
    NODE_TYPE_LEAF = -1,
    NODE_TYPE_ROOT =  0,
    /** Intermediário */
    NODE_TYPE_INTM =  1,
};

/**
 * Definição da struct com dados necessários para manipular a
 * árvore B, bem como outros metadados presentes no registro
 * de cabeçalho.
 *
 * O nó raiz é guardado em memória primária e atualizado em
 * disco somente quando necessário, permitindo que haja um
 * acesso a disco a menos para a busca e inserção, além de
 * simplificar o código, uma vez que não é preciso tratar
 * o caso em que a árvore está vazia (podemos "fingir" que
 * há sempre um nó raiz).
 */
struct b_tree_index {
    /** Arquivo de dados da árvore */
    FILE *file;
    
    /** RRN da próxima página disponível no arquivo de dados da árvore */
    uint32_t next_rrn;
    
    /** RRN da página que contém o nó raiz da árvore */
    uint32_t root_rrn;

    /** Quantidade de páginas usadas pela árvore */ 
    uint32_t n_pages;
    
    /**
     * Indica se a página do nó raiz contida em `root` está
     * "suja", ou seja, foi modificada e deve ser reescrita
     * no arquivo de dados.
     */
    bool root_dirty;

    /**
     * Se o arquivo de dados da árvore B foi aberto de modo
     * a permitir modificação.
     */
    bool mode_is_modify;

    /** Status do arquivo de dados da árvore */
    uint8_t status;

    /** Conteúdo da página do nó raiz */
    b_tree_page_t root;
};

/**
 * Campos do registro de cabeçalho, na ordem
 * em que aparecem na árvore B.
 */
#define HEADER_FIELDS(X)  \
    X(uint8_t,  status)   \
    X(uint32_t, root_rrn) \
    X(uint32_t, next_rrn) \
    X(uint32_t, n_pages)

#define HEADER_SIZE (1 /* sizeof status */ + 4 /* sizeof root_rrn */ + 4 /* sizeof next_rrn */ + 4 /* sizeof n_pages */)

#define HEADER_PAGE_NEEDS_PADDING HEADER_SIZE < PAGE_SIZE

/** Tipos "packed"/"empacotados"; vd. `defs.h` */
#define PACKED(T) packed_##T

#define X(T, name) T name;

typedef struct {
    HEADER_FIELDS(X)
} __attribute__((packed)) PACKED(b_header_t);

typedef struct {
    SUBNODE_FIELDS(X)
} __attribute__((packed)) PACKED(b_tree_subnode_t);

#undef X

/** Para esse trabalho, a ordem da árvore deve ser 3 */
static_assert(TREE_ORDER == 3, "TREE_ORDER");

/**
 * Verifica se as macros `SIZE_LEFT` e `SUBNODE_SKIP`
 * foram definidas corretamente
 */
static_assert(SIZE_LEFT == sizeof ((b_tree_subnode_t *)0)->left, "SIZE_LEFT");
static_assert(SIZE_LEFT + SUBNODE_SKIP == sizeof(PACKED(b_tree_subnode_t)), "SIZE_LEFT + SUBNODE_SKIP");

/**
 * Retorna o RRN da próxima página disponível e a incrementa.
 * Essa função não aloca espaço para a página e não escreve
 * no arquivo de dados da árvore B.
 */
static uint32_t b_tree_new_page(b_tree_index_t *tree)
{
    uint32_t rrn = tree->next_rrn++;
    tree->n_pages++;

    return rrn;
}

/**
 * Inicializa a página `page` com valores padrão. Para o
 * tipo do nó, usa `NODE_TYPE_LEAF`.
 */
static void b_tree_init_page(b_tree_page_t *page)
{
    char *p = NODE_DATA_P(page);
    const char *end = &page->data[sizeof page->data];

    *NODE_TYPE_P(page) = NODE_TYPE_LEAF;
    *NODE_LEN_P(page) = 0;

    // Inicializa o campo `left`
    memset(p, -1, SIZE_LEFT);
    p += SIZE_LEFT;
    
    // Inicializa os demais campos (o valor padrão para todos é -1)
    while (p + SUBNODE_SKIP <= end) {
        memset(p, -1, SUBNODE_SKIP);
        p += SUBNODE_SKIP;
    }

#if TREE_PAGE_NEEDS_PADDING
    // Preenche o espaço que sobrar na página com '$'
    for (char *q = p; q != end; q++)
        *q = '$';
#endif
}

/** Lê o registro de cabeçalho da árvore B, a partir do arquivo de dados. */
static bool b_tree_read_header(b_tree_index_t *tree)
{
    fseek(tree->file, 0L, SEEK_SET);

    #define X(_, name) FAIL_IF(fread(&tree->name, sizeof tree->name, 1, tree->file) != 1)

    HEADER_FIELDS(X)

    #undef X

    return true;
}

/** Escreve os dados do registro de cabeçalho da árvore B no arquivo de dados. */
static bool b_tree_write_header(b_tree_index_t *tree)
{
    fseek(tree->file, 0L, SEEK_SET);

    #define X(_, name) FAIL_IF(fwrite(&tree->name, sizeof tree->name, 1, tree->file) != 1)

    HEADER_FIELDS(X)

    #undef X

#if HEADER_PAGE_NEEDS_PADDING
    for (size_t i = HEADER_SIZE; i < PAGE_SIZE; i++)
        fputc('$', tree->file);
#endif

    return true;
}

/**
 * Lê uma página da árvore B cujo RRN é dado por `rrn`, a partir do arquivo de dados,
 * armazenando-a em `page`.
 */
static void b_tree_read_page(b_tree_index_t *tree, uint32_t rrn, b_tree_page_t *page)
{
    // Adicionamos 1 devido ao registro de cabeçalho
    fseek(tree->file, (rrn + 1) * PAGE_SIZE, SEEK_SET);
    fread(page, PAGE_SIZE, 1, tree->file);
}

/**
 * Escreve uma página da árvore B, cujo conteúdo está em `page`, no arquivo de dados,
 * usando o RRN `rrn`.
 */
static void b_tree_write_page(b_tree_index_t *tree, uint32_t rrn, b_tree_page_t *page)
{
    // Adicionamos 1 devido ao registro de cabeçalho
    fseek(tree->file, (rrn + 1) * PAGE_SIZE, SEEK_SET);
    fwrite(page, PAGE_SIZE, 1, tree->file);
}

b_tree_index_t *b_tree_open(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);

    if (!file)
        return NULL;

    b_tree_index_t *tree = malloc(sizeof *tree);

    tree->file = file;
    tree->root_dirty = false;
    tree->mode_is_modify = strchr(mode, 'w') || strchr(mode, '+');

    if (!b_tree_read_header(tree)) {
        // Assumimos que o arquivo está vazio e o inicializamos.
        // 
        // Se o arquivo não foi aberto de modo a permitir modificação
        // e não possui cabeçalho válido, isso é considerado um erro
        if (!tree->mode_is_modify) {
            b_tree_close(tree);
            return NULL;
        }

        tree->status = B_STATUS_INCONSISTENT;

        tree->next_rrn = 0;
        tree->root_rrn = -1;
        tree->n_pages = 0;

        b_tree_write_header(tree);
        b_tree_init_page(&tree->root);
    } else {
        if (tree->status == B_STATUS_INCONSISTENT) {
            b_tree_close(tree);
            return NULL;
        }

        b_tree_read_page(tree, tree->root_rrn, &tree->root);

        if (tree->mode_is_modify) {
            tree->status = B_STATUS_INCONSISTENT;

            fseek(tree->file, offsetof(PACKED(b_header_t), status), SEEK_SET);
            fwrite(&tree->status, sizeof tree->status, 1, tree->file);
        }
    }

    return tree;
}

void b_tree_close(b_tree_index_t *tree)
{
    if (tree->root_dirty)
        b_tree_write_page(tree, tree->root_rrn, &tree->root);

    if (tree->mode_is_modify) {
        tree->status = B_STATUS_CONSISTENT;
        b_tree_write_header(tree);
    }

    fclose(tree->file);
    free(tree);
}

/**
 * Retorna um ponteiro para uma página da árvore B, que irá corresponder à página
 * do nó raiz em `tree->root` ou a `non_root`, dependendo do RRN desejado. Se a
 * página desejada não correponder ao nó raiz, lê o conteúdo desse nó a partir do
 * disco e o armazena na página retornada.
 */
static inline b_tree_page_t *b_tree_adequate_page(b_tree_index_t *const tree,
                                                  uint32_t page_rrn, b_tree_page_t *non_root)
{
    if (page_rrn == tree->root_rrn)
        return &tree->root;

    b_tree_read_page(tree, page_rrn, non_root);
    return non_root;
}

/**
 * Obtém o subnó da página `page` com índice `index` e o armazena em `*sub`.
 */
static void b_tree_get_subnode(const b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub)
{
    const char *base = NODE_DATA_P(page) + SUBNODE_SKIP * index;
 
    #define X(T, name) \
        memcpy(&sub->name, base + offsetof(PACKED(b_tree_subnode_t), name), sizeof sub->name);

    SUBNODE_FIELDS(X)

    #undef X
}

/**
 * Armazena na posição `index` da página `page` o subnó `sub`.
 */
static void b_tree_put_subnode(b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub)
{
    char *base = NODE_DATA_P(page) + SUBNODE_SKIP * index;

    #define X(T, name) \
        memcpy(base + offsetof(PACKED(b_tree_subnode_t), name), &sub->name, sizeof sub->name);

    SUBNODE_FIELDS(X)

    #undef X
}

/**
 * Realiza uma busca binária, procurando pela chave `key` na página `page`.
 *
 * Armazena em `*sub` o subnó que contém a chave, se for encontrada, ou onde
 * um valor com a chave `key` deveria ser inserido, se não for encontrada.
 *
 * Retorna o índice do subnó na página.
 *
 * O comportamento dessa função é indefinido se `*NODE_LEN_P(page) == 0`.
 */
static uint32_t b_tree_bin_search(const b_tree_page_t *page, uint32_t key, b_tree_subnode_t *sub)
{
    // A busca é realizada no intervalo [low, high), comparando
    // o valor do meio `mid` com o valor sendo buscado. Desse modo,
    // a quantidade de comparações realizada é reduzida quando
    // comparado a uma implementação que realiza duas comparações
    // (obviamente, a complexidade de tempo é a mesma).
    //
    // Além disso, a busca em um intervalo semi-aberto permite que
    // a busca retorne a posição onde o valor deveria ser inserido
    // (onde subentende-se que inserir corresponde a guardar o valor
    // naquela posição e deslocar as demais para a direita), sendo
    // que essa implementação funciona até mesmo se o local de
    // inserção for o fim do vetor, o que é útil na implementação
    // das operações inserção e split da árvore B.
    uint32_t low = 0;
    uint32_t high = *NODE_LEN_P(page);

    uint32_t mid = low + (high - low) / 2;

    while (low < high) {
        // O valor lido para `sub` aqui será, ao final do loop, o valor
        // correto, pois a condição de parada `low >= high` indica, para
        // um intervalo semi-aberto, que o valor não foi encontrado; e os
        // valores de `low` e `high` que levaram a essa condição já foram
        // calculados na iteração anterior e já foram usados para calcular
        // o novo valor de `mid`.
        b_tree_get_subnode(page, mid, sub);

        if (key > sub->key)
            low = mid + 1;
        else if (key < sub->key)
            high = mid;
        else
            return mid;

        mid = low + (high - low) / 2;
    }

    return mid;
}

/** Implementa a busca de fato, de forma recursiva; vd. `b_tree_search`. */
static bool b_tree_search_impl(b_tree_index_t *const tree, int32_t page_rrn, uint32_t key, uint64_t *offset)
{
    if (page_rrn == -1)
        return false;
    
    // Usa `tree->root` como página se `page_rrn == tree->root_rrn`, caso contrário
    // usa uma página anônima alocada na stack ("compound literal") e lê o conteúdo
    // da página com o RRN desejado a partir do disco.
    const b_tree_page_t *page = b_tree_adequate_page(tree, page_rrn, &(b_tree_page_t){});

    size_t len = *NODE_LEN_P(page);

    // Esse caso só irá ocorrer no início, quando a
    // árvore estiver vazia, porém deve ser tratado.
    if (len == 0)
        return false;

    b_tree_subnode_t sub;
    uint32_t index = b_tree_bin_search(page, key, &sub);

    // Se a posição determinada pela busca binária for o fim, a chave não foi encontrada
    if (index == len)
        return false;

    if (key > sub.key)
        return b_tree_search_impl(tree, sub.right, key, offset);

    if (key < sub.key)
        return b_tree_search_impl(tree, sub.left, key, offset);

    *offset = sub.offset;
    return true;
}

bool b_tree_search(b_tree_index_t *tree, uint32_t key, uint64_t *offset)
{
    return b_tree_search_impl(tree, tree->root_rrn, key, offset);
}

/**
 * Insere o subnó `sub` na posição `index` da página `page` da árvore B,
 * deslocando os nós à direita, se necessário, e incrementando o tamanho
 * (quantidade de nós armazenados) da página.
 *
 * Deve haver espaço para mais um nó na página. Caso não haja, o
 * comportamento dessa função é indefinido.
 */ 
static void b_tree_shift_insert_subnode(b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub)
{
    char *base = NODE_DATA_P(page) + SUBNODE_SKIP * index;

    char *src = base;
    char *dest = base + SUBNODE_SKIP;

    size_t len = SIZE_LEFT + SUBNODE_SKIP * (*NODE_LEN_P(page) - index);

    memmove(dest, src, len);

    b_tree_put_subnode(page, index, sub);

    *NODE_LEN_P(page) += 1;
}

/**
 * Realiza a operação "split" de uma página `page` da árvore B em duas,
 * redistribuindo as chaves uniformemente e promovendo uma chave. Deve
 * haver espaço para uma página da árvore B em `new`, e `ins_index` deve
 * ser o índice onde será inserida uma chave.
 *
 * Guarda em `*promoted` os dados da chave que foi promovida.
 *
 * Retorna o RRN da nova página.
 */
int32_t b_tree_split_page(b_tree_index_t *tree, b_tree_page_t *page, b_tree_page_t *new,
                          uint32_t ins_index, b_tree_subnode_t *sub, b_tree_subnode_t *promoted)
{
    int32_t new_rrn = b_tree_new_page(tree);
    b_tree_init_page(new);

    if (*NODE_TYPE_P(page) == NODE_TYPE_ROOT)
        *NODE_TYPE_P(page) = NODE_TYPE_INTM;

    *NODE_TYPE_P(new) = *NODE_TYPE_P(page);

    // O nó da esquerda deve sempre ficar com uma chave
    // a mais, se a quantidade de chaves for par
    //
    // NOTE: uma das chaves é promovida, logo, não entra
    // na contagem da quantidade de chaves que serão
    // redistribuídas entre os dois nós. No entanto, há
    // outra chave que está sendo inserida nesse nó
    // (armazenada em `sub`) que deve ser considerada.
    static const uint32_t len_right = N_KEYS / 2;
    static const uint32_t len_left = N_KEYS - len_right;

    *NODE_LEN_P(new) = len_right;

    // Verifica se o novo nó a ser inserido deverá ficar
    // na página à esquerda (`page`) ou à direita (`new`)
    if (ins_index < len_left) {
        // Promove o nó que seria o primeiro nó da direita se o split
        // e a promoção fossem realizados em etapas separadas
        b_tree_get_subnode(page, len_left - 1, promoted);

        char *const src = NODE_DATA_P(page) + len_left * SUBNODE_SKIP;
        char *const dest = NODE_DATA_P(new);

        size_t len = SIZE_LEFT + len_right * SUBNODE_SKIP;

        // Copia a parte da página da esquerda que agora
        // deve ficar à direita para a página da direita
        memcpy(dest, src, len);
        // Queremos apagar a chave promovida
        memset(src - SUBNODE_SKIP, -1, len + SUBNODE_SKIP);

        // Essa quantidade será corrigida pela função `b_tree_shift_insert_subnode`
        *NODE_LEN_P(page) = len_left - 1;

        b_tree_shift_insert_subnode(page, ins_index, sub);

        return new_rrn;
    }

    *NODE_LEN_P(page) = len_left;

    // Índice onde a inserção deveria ocorrer (já determinamos
    // que a inserção deve ocorrer na página à direita, `new`)
    // se a inserção e a promoção fossem etapas separadas, com
    // a promoção e o deslocamento ocorrendo após a inserção.
    ins_index -= len_left;

    // Origem (região de `page` a ser copiada para `new`),
    // assumindo que o split e a promoção sejam realizados
    // separadamente (embora esse código implemente as duas
    // operações simultaneamente, para reduzir a quantidade
    // de cópias, tendo que lidar com mais casos)
    char *const src = NODE_DATA_P(page) + SUBNODE_SKIP * len_left;
    // Destino (região de `new` que receberá o conteúdo copiado de `page`)
    char *const dest = NODE_DATA_P(new);

    // Tamanho da região a ser copiada
    size_t len = SIZE_LEFT + len_right * SUBNODE_SKIP;

    if (ins_index == 0) {
        // O nó que recebemos para inserção foi o nó escolhido para promoção;
        // promovemos esse nó diretamente, sem realizar a inserção
        memcpy(promoted, sub, sizeof *sub);

        // Copia a parte da página da esquerda que agora
        // deve ficar à direita para a página da direita
        memcpy(dest, src, len);
    } else {
        // Promove o nó que seria o primeiro nó da direita se o split
        // e a promoção fossem realizados em etapas separadas
        b_tree_get_subnode(page, len_left, promoted);

        // Devemos decrementar o índice de inserção, pois estamos
        // realizando a promoção antes de fazer a inserção
        ins_index--;

        // Realiza duas cópias separadas, da parte "predecessora"
        // e da parte "sucessora" ao índice de inserção, deixando
        // um espaço em `ins_index` na página à direita para que
        // seja possível realizar a inserção. Dessa forma, copiamos
        // cada chave apenas uma vez, em vez de duas (caso fosse
        // feita uma cópia convencional seguida de um deslocamento)
        //
        // NOTE: adicionamos `SUBNODE_SKIP` para pular o subnó que
        // foi promovido, que se encontra na posição `len_left`
        char *const src_prec = src + SUBNODE_SKIP;
        char *const dest_prec = dest;
        
        size_t len_prec = ins_index
                              ? SIZE_LEFT + ins_index * SUBNODE_SKIP
                              : 0;

        memcpy(dest_prec, src_prec, len_prec);

        char *src_succ = src_prec + len_prec;
        // Adicionamos `SUBNODE_SKIP` para deixar espaço para inserir o subnó em `ins_index`
        char *dest_succ = dest_prec + len_prec + SUBNODE_SKIP;

        // Subtraímos 1 do tamanho para considerar o subnó que será inserido em `ins_index`
        size_t len_succ = len_right - ins_index - 1 > 0
                              ? SIZE_LEFT + (len_right - ins_index - 1) * SUBNODE_SKIP
                              : 0;

        memcpy(dest_succ, src_succ, len_succ);

        // Realiza a inserção em `ins_index`
        b_tree_put_subnode(new, ins_index, sub);
    }

    // Apaga a parte da página `page` que foi copiada para `new`
    // 
    // NOTE: adicionamos `SIZE_LEFT` para que o filho direito do
    // último subnó não seja apagado
    memset(src + SIZE_LEFT, -1, len - SIZE_LEFT);
    
    return new_rrn;
}

/**
 * Implementa a inserção de fato, de forma recursiva; vd. `b_tree_insert`.
 *
 * Retorna `false` se não houve promoção para o nível anterior.
 */
static bool b_tree_insert_impl(b_tree_index_t *const tree, int32_t page_rrn, uint32_t key, uint64_t offset, b_tree_subnode_t *promoted)
{
    b_tree_page_t *page = b_tree_adequate_page(tree, page_rrn, &(b_tree_page_t){});

    size_t len = *NODE_LEN_P(page);

    if (*NODE_TYPE_P(page) == NODE_TYPE_LEAF) {
        b_tree_subnode_t sub = { .left = -1, .right = -1 };

        // Índice onde a inserção irá ocorrer, se houver espaço
        uint32_t ins_index = len ? b_tree_bin_search(page, key, &sub) : 0;

        sub.key = key;
        sub.offset = offset;

        if (len < N_KEYS) {
            sub.left = -1;
            // << Queremos preservar o valor de sub.right

            // Insere ordenado
            b_tree_shift_insert_subnode(page, ins_index, &sub);

            if (page == &tree->root)
                tree->root_dirty = true;
            else
                b_tree_write_page(tree, page_rrn, page);

            return false;
        } else {
            // << Queremos preservar os valores de sub.left e sub.right

            b_tree_page_t new;

            // Split
            int32_t new_rrn = b_tree_split_page(tree, page, &new, ins_index, &sub, promoted);

            promoted->left = page_rrn;
            promoted->right = new_rrn;

            b_tree_write_page(tree, page_rrn, page);
            b_tree_write_page(tree, new_rrn, &new);

            return true;
        }
    }

    // Esse caso nunca deveria ocorrer
    assert(len != 0);

    b_tree_subnode_t sub;
    // Índice onde a inserção deveria ocorrer, se esse nó fosse um nó folha
    uint32_t ins_index = b_tree_bin_search(page, key, &sub);

    // Devemos guardar o subnó anterior à posição onde a inserção seria
    // realizada para obter seu filho direito (por onde iremos prosseguir
    // para realizar a inserção).
    //
    // NOTE: Uma vez que a função `b_tree_bin_search` retorna a posição onde a
    // inserção deveria ser realizada (deslocando os demais à direita), o único
    // caso em que iremos para o filho direito em vez do esquerdo é quando formos
    // inserir no fim, ou seja, quando `ins_index == len`.
    if (ins_index == len)
        b_tree_get_subnode(page, len - 1, &sub);

    int32_t next_rrn;

    if (key > sub.key)
        next_rrn = sub.right;
    else if (key < sub.key)
        next_rrn = sub.left;
    else
        return false;

    bool was_promoted = b_tree_insert_impl(tree, next_rrn, key, offset, promoted);

    if (!was_promoted)
        return false;

    if (len < N_KEYS) {
        // Insere ordenado
        b_tree_shift_insert_subnode(page, ins_index, promoted);

        if (page == &tree->root)
            tree->root_dirty = true;
        else
            b_tree_write_page(tree, page_rrn, page);

        return false;
    } else {
        b_tree_page_t new;

        // Iremos realizar outra promoção, porém ainda temos que fazer o split
        //
        // Como já não precisamos de `sub` (originalmente usado para a inserção, no entanto
        // a inserção já foi feita --- estamos na volta da recursão), copiamos o nó promovido
        // de um nível inferior para `sub` e realizamos o "split" (que já irá inserir `sub`
        // na posição desejada)
        memcpy(&sub, promoted, sizeof sub);

        // Split
        int32_t new_rrn = b_tree_split_page(tree, page, &new, ins_index, &sub, promoted);

        promoted->left = page_rrn;
        promoted->right = new_rrn;

        b_tree_write_page(tree, page_rrn, page);
        b_tree_write_page(tree, new_rrn, &new);

        return true;
    }
}

void b_tree_insert(b_tree_index_t *tree, uint32_t key, uint64_t offset)
{
    if (tree->root_rrn == -1)
        tree->root_rrn = b_tree_new_page(tree);

    b_tree_subnode_t promoted;
    bool was_promoted = b_tree_insert_impl(tree, tree->root_rrn, key, offset, &promoted);

    if (was_promoted) {
        tree->root_rrn = b_tree_new_page(tree);
        b_tree_init_page(&tree->root);

        *NODE_TYPE_P(&tree->root) = NODE_TYPE_ROOT;

        b_tree_shift_insert_subnode(&tree->root, 0, &promoted);
        tree->root_dirty = true;
    }
}
