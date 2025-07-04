#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdalign.h>

#include "util/mem.h"
#include "index/b_tree.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

#define ABS(x) ((x) >= 0 ? (x) : -(x))

/**
 * Implementação de uma árvore B genérica, flexível quanto ao tamanho
 * da página: basta alterar o valor da macro `PAGE_SIZE` para usar um
 * tamanho diferente de página de disco.
 */

#define PAGE_SIZE 44

#define B_STATUS_INCONSISTENT '0'
#define B_STATUS_CONSISTENT   '1'

// SYNC: b_subnode
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

/** Taxa de ocupação mínima de um nó intermediário (não-raiz e não-folha) */
#define MIN_OCCUPANCY_INTM (N_KEYS / 2)

/** Taxa de ocupação mínima de um nó folha */
#define MIN_OCCUPANCY_LEAF (N_KEYS / 2 + N_KEYS % 2)

/** Ordem da árvore */
#define TREE_ORDER (N_KEYS + 1)

/** Verdadeiro se houver espaço no final da página da árvore, que deverá ser preenchido com '$' */
#define TREE_PAGE_NEEDS_PADDING N_KEYS * SUBNODE_SKIP + SIZE_LEFT < PAGE_SIZE - PAGE_META_SIZE

/**
 * Guarda uma página (nó) da árvore B. O conteúdo da página é simplesmente
 * representado como um `char[]`, para que seja possível usar a mesma
 * representação para a página da árvore B em memória primária e em disco
 * (graças ao `alignas(uint32_t)`, valores inteiros podem ser lidos/escritos
 * diretamente na página, desde que sigam o alinhamento, validando as macros
 * a seguir).
 */
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

enum b_node_type {
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

typedef struct b_hook b_hook_t;

struct b_hook {
    enum b_hook_type type;
    b_hook_cb_t *cb;
    void *data;
    b_hook_t *next;
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

    b_hook_t *hooks;

    /** RRN da página que contém o nó raiz da árvore */
    uint32_t root_rrn;

    /** RRN da próxima página disponível no arquivo de dados da árvore */
    uint32_t next_rrn;

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

/**
 * Inicializador padrão para um subnó.
 *
 * WARN: não use os campos `key`/`offset` de um subnó inicializado
 * por meio dessa macro sem antes inicializá-los separadamente.
 */
#define SUBNODE_INIT { .left = -1, .right = -1 }

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
    // Suprimir warnings `-Wunused-result` no runcodes
    int _ = fread(page, PAGE_SIZE, 1, tree->file);
    (void)_;
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

/**
 * Escreve a página da árvore B contida em `page` no arquivo de dados, exceto se a
 * página corresponder à raiz da árvore: nesse caso, a página é marcada como
 * modificada, para que possa ser escrita posteriormente.
 */
static void b_tree_write_page_or_mark_dirty(b_tree_index_t *tree, uint32_t rrn, b_tree_page_t *page) {
    if (page == &tree->root)
        tree->root_dirty = true;
    else
        b_tree_write_page(tree, rrn, page);
}

b_tree_index_t *b_tree_open(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);

    if (!file)
        return NULL;

    b_tree_index_t *tree = malloc(sizeof *tree);

    tree->file = file;
    tree->hooks = NULL;
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

        tree->root_rrn = -1;
        tree->next_rrn = 0;
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

    for (b_hook_t *curr = tree->hooks; curr; curr = curr->next) {
        if (curr->type == B_HOOK_CLOSE)
            curr->cb(tree->file, curr->data);
    }

    b_hook_t *next = NULL;
    b_hook_t *prev = tree->hooks;

    while (prev || next) {
        next = prev->next;
        free(prev);
        prev = next;
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

/** Retorna o filho esquerdo do subnó com índice `n` na página `p` (`b_tree_page_t *`) */ 
#define LEFT_CHILD_OF(p, n) *(uint32_t *)(NODE_DATA_P(p) + SUBNODE_SKIP * (n))
/** Retorna o filho direito do subnó com índice `n` na página `p` (`b_tree_page_t *`) */
#define RIGHT_CHILD_OF(p, n) LEFT_CHILD_OF(p, n + 1)

/**
 * Flags para as operações `b_tree_get_subnode` e `b_tree_put_subnode`, determinando
 * quais campos do subnó são lidos/escritos.
 *
 * `SUB_KEY`: `key` e `offset`
 * `SUB_L`:   `left`
 * `SUB_R`:   `right`
 * `SUB_CLD`: `left` e `right`
 */
#define SUB_KEY (1 << 0)
#define SUB_L   (1 << 1)
#define SUB_R   (1 << 2)
#define SUB_CLD (SUB_L | SUB_R)

/**
 * Obtém os campos do subnó da página `page` com índice `index` especificados pelo parâmetro `flags`,
 * armazenando-os em `*sub`. Se `SUB_CLD` for especificado, os campos referentes aos filhos esquerdo
 * e direito serão lidos. Se `SUB_KEY` for especificado, os campos referentes à chave (`key`) e ao
 * offset (`offset`) serão lidos.
 */
static void b_tree_get_subnode(const b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub, int flags)
{
    // SYNC: b_subnode
    const char *base = NODE_DATA_P(page) + SUBNODE_SKIP * index;

    if (flags & SUB_L)
        memcpy(&sub->left, base + offsetof(PACKED(b_tree_subnode_t), left), sizeof sub->left);

    if (flags & SUB_KEY) {
        memcpy(&sub->key, base + offsetof(PACKED(b_tree_subnode_t), key), sizeof sub->key);
        memcpy(&sub->offset, base + offsetof(PACKED(b_tree_subnode_t), offset), sizeof sub->offset);
    }

    if (flags & SUB_R)
        memcpy(&sub->right, base + offsetof(PACKED(b_tree_subnode_t), right), sizeof sub->right);
}

/**
 * Armazena na posição `index` da página `page` o subnó `sub`. Os valores possíveis para o parâmetro
 * `flags` têm significado análogo ao de `b_tree_get_subnode`.
 */
static void b_tree_put_subnode(b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub, int flags)
{
    char *base = NODE_DATA_P(page) + SUBNODE_SKIP * index;

    if (flags & SUB_L)
        memcpy(base + offsetof(PACKED(b_tree_subnode_t), left), &sub->left, sizeof sub->left);

    if (flags & SUB_KEY) {
        memcpy(base + offsetof(PACKED(b_tree_subnode_t), key), &sub->key, sizeof sub->key);
        memcpy(base + offsetof(PACKED(b_tree_subnode_t), offset), &sub->offset, sizeof sub->offset);
    }

    if (flags & SUB_R)
        memcpy(base + offsetof(PACKED(b_tree_subnode_t), right), &sub->right, sizeof sub->right);
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
    // o valor do meio `mid` com o valor sendo buscado. Usar um
    // intervalo semi-aberto permite que a busca retorne a posição
    // onde o valor deveria ser inserido (onde subentende-se que
    // inserir corresponde a guardar o valor naquela posição e
    // deslocar as demais para a direita), sendo que essa
    // implementação funciona até mesmo se o local de inserção for
    // o fim do vetor, o que é útil na implementação das operações
    // inserção e split da árvore B.
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
        b_tree_get_subnode(page, mid, sub, SUB_KEY | SUB_CLD);

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

    // Se a posição determinada pela busca binária for o fim, devemos seguir pelo
    // filho direito do último subnó (posição `len - 1`), em vez do filho esquerdo
    // de um subnó não existente na posição `len` --- que é o subnó retornado pela
    // função `b_tree_bin_search`, embora ambos correspondam ao mesmo filho. Isso
    // porque se `len == N_KEYS`, tentar ler esse subnó resultaria em um buffer
    // overflow. Seria possível ler apenas o filho esquerdo desse subnó usando
    // `LEFT_CHILD_OF`, porém essa solução é mais clara.
    //
    // NOTE: a função `b_tree_bin_search` foi definida considerando que a posição
    // de inserção é a posição do primeiro subnó que será deslocado à direita, o
    // que corresponde a dizer que é a posição do primeiro subnó cuja chave é maior
    // que ou igual a `key`. Ou seja, dado que a busca binária retornou um nó válido,
    // sempre prosseguimos pelo filho esquerdo desse nó.   vd. `b_tree_insert_impl`.
    if (index == len)
        b_tree_get_subnode(page, len - 1, &sub, SUB_KEY | SUB_CLD);

    if (key < sub.key)
        return b_tree_search_impl(tree, sub.left, key, offset);
    else if (key > sub.key)
        return b_tree_search_impl(tree, sub.right, key, offset);

    // SYNC: b_subnode
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

    b_tree_put_subnode(page, index, sub, SUB_KEY | SUB_CLD);

    *NODE_LEN_P(page) += 1;
}

/**
 * Copia `n` subnós de `src` para `dest`.
 *
 * Não deve haver sobreposição entre as regiões de memória apontadas por `src` e `dest`.
 */
static char *b_tree_copy_subnodes(char *restrict dest, char *restrict src, uint32_t n)
{
    size_t len = SIZE_LEFT + n * SUBNODE_SKIP;
    memcpy(dest, src, len);

    return src + len;
}

enum which_skip {
    SKIP_SRC,
    SKIP_DEST,
    SKIP_NONE
};

/**
 * Copia `n - 1` subnós de `src` para `dest`. Pula o subnó na posição `skip_index`,
 * em `src` ou `dest`, dependendo do valor de `which`. Mesmo que um subnó seja pulado
 * em `src`, os subnós copiados ainda estarão dispostos sequencialmente em `dest` e
 * vice-versa. Para tal, é necessário realizar duas cópias separadas.
 *
 * Se `which == SKIP_NONE`, faz uma única cópia de `n` subnós (em vez de `n - 1`),
 * chamando `b_tree_copy_subnodes`. Se `which` não for `SKIP_NONE`, sempre copia pelo
 * menos um filho de `src` para `dest`.
 *
 * Não deve haver sobreposição entre as regiões de memória apontadas por `src` e `dest`.
 */
static char *b_tree_copy_subnodes_skipping_over(char *restrict dest, char *restrict src, uint32_t n,
                                               uint32_t skip_index, enum which_skip which)
{
    if (which == SKIP_NONE)
        return b_tree_copy_subnodes(dest, src, n);

    // Realiza duas cópias separadas, da parte "predecessora"
    // e da parte "sucessora" ao índice a ser pulado.
    size_t len_prec = SIZE_LEFT + skip_index * SUBNODE_SKIP;

    memcpy(dest, src, len_prec);

    char *src_succ = src + len_prec;
    char *dest_succ = dest + len_prec;

    if (which == SKIP_SRC)
        src_succ += SUBNODE_SKIP;
    else
        dest_succ += SUBNODE_SKIP;

    // Subtraímos 1 pois queremos a quantidade de subnós após `skip_index`,
    // não incluindo o próprio subnó nessa posição
    size_t len_succ = n - skip_index - 1 > 0
                          ? SIZE_LEFT + (n - skip_index - 1) * SUBNODE_SKIP
                          : 0;

    memcpy(dest_succ, src_succ, len_succ);

    return src_succ + len_succ;
}

/**
 * Realiza a operação "split" de uma página `page` da árvore B em duas,
 * redistribuindo as chaves uniformemente e promovendo uma chave. Deve
 * haver espaço para uma página da árvore B em `new`, e `ins_index` deve
 * ser o índice onde será inserido o subnó `sub`.
 *
 * Guarda em `*promoted` os dados da chave que foi promovida.
 *
 * Retorna o RRN da nova página.
 */
int32_t b_tree_split(b_tree_index_t *tree, b_tree_page_t *page, b_tree_page_t *new,
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
        b_tree_get_subnode(page, len_left - 1, promoted, SUB_KEY);

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

    // Quantidade de subnós a serem copiados
    uint32_t n = len_right;

    // Nesse caso, o filho esquerdo do subnó recebido para inserção
    // deve ficar em em `page`, e o filho direito em `new`
    //
    // No entanto, o último filho de `page` já é igual ao filho esquerdo,
    // logo, nos preocupamos apenas com o filho direito
    if (ins_index == 0) {
        // O subnó que recebemos para inserção foi o subnó escolhido para
        // promoção; promovemos esse subnó diretamente, sem realizar a inserção
        memcpy(promoted, sub, sizeof *sub);

        // Copia a parte da página da esquerda que agora
        // deve ficar à direita para a página da direita
        b_tree_copy_subnodes(dest, src, n);

        // XXX: ...?
        LEFT_CHILD_OF(new, 0) = sub->right;
    } else {
        // Promove o nó que seria o primeiro nó da direita se o split
        // e a promoção fossem realizados em etapas separadas
        b_tree_get_subnode(page, len_left, promoted, SUB_KEY);

        // Devemos decrementar o índice de inserção, pois estamos
        // realizando a promoção antes de fazer a inserção
        ins_index--;

        // Adicionamos `SUBNODE_SKIP` para pular o subnó que foi promovido,
        // que se encontra na posição `len_left`; essa função é usada pois
        // ainda devemos pular o índice `ins_index` para realizar a inserção
        b_tree_copy_subnodes_skipping_over(dest, src + SUBNODE_SKIP, n, ins_index, SKIP_DEST);

        // Realiza a inserção em `ins_index`
        b_tree_put_subnode(new, ins_index, sub, SUB_KEY | SUB_CLD);
    }

    size_t len = SIZE_LEFT + n * SUBNODE_SKIP;

    // Apaga a parte da página `page` que foi copiada para `new`
    // 
    // NOTE: adicionamos `SIZE_LEFT` para que o filho direito do
    // último subnó que permaneceu em `page` não seja apagado
    memset(src + SIZE_LEFT, -1, len - SIZE_LEFT);

    return new_rrn;
}

/**
 * Realiza a inserção de `sub` na posição `ins_index` da página `page`, cujo RRN
 * é `page_rrn`, escrevendo o que for necessário de volta no arquivo de dados.
 *
 * Se houver split e promoção, retorna `true` e armazena o subnó promovido em `*promoted`.
 */
static bool b_tree_perform_insert(b_tree_index_t *const tree, int32_t page_rrn, b_tree_page_t *page,
                                  uint32_t ins_index, b_tree_subnode_t *sub, b_tree_subnode_t *promoted)
{
    size_t len = *NODE_LEN_P(page);

    if (len < N_KEYS) {
        // Insere ordenado
        b_tree_shift_insert_subnode(page, ins_index, sub);

        b_tree_write_page_or_mark_dirty(tree, page_rrn, page);

        return false;
    } else {
        b_tree_page_t new;

        // Split
        int32_t new_rrn = b_tree_split(tree, page, &new, ins_index, sub, promoted);

        promoted->left = page_rrn;
        promoted->right = new_rrn;

        b_tree_write_page(tree, page_rrn, page);
        b_tree_write_page(tree, new_rrn, &new);

        return true;
    }
}

/**
 * Implementa a inserção de fato, de forma recursiva; vd. `b_tree_insert`.
 *
 * Se houver split e promoção, retorna `true` e armazena o subnó promovido em `*promoted`.
 */
static bool b_tree_insert_impl(b_tree_index_t *const tree, int32_t page_rrn, uint32_t key, uint64_t offset, b_tree_subnode_t *promoted)
{
    b_tree_page_t *page = b_tree_adequate_page(tree, page_rrn, &(b_tree_page_t){});

    size_t len = *NODE_LEN_P(page);

    if (*NODE_TYPE_P(page) == NODE_TYPE_LEAF) {
        b_tree_subnode_t sub = SUBNODE_INIT;

        // Índice onde a inserção irá ocorrer, se houver espaço
        uint32_t ins_index = len ? b_tree_bin_search(page, key, &sub) : 0;

        // A chave já existe na árvore: atualizamos o offset em
        // vez de inserir uma nova chave ou retornar um erro
        //
        // SYNC: b_subnode
        if (ins_index < len && key == sub.key) {
            sub.offset = offset;

            b_tree_put_subnode(page, ins_index, &sub, SUB_KEY);
            b_tree_write_page_or_mark_dirty(tree, page_rrn, page);

            return false;
        }

        // SYNC: b_subnode
        sub.key = key;
        sub.offset = offset;

        return b_tree_perform_insert(tree, page_rrn, page, ins_index, &sub, promoted);
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
        b_tree_get_subnode(page, len - 1, &sub, SUB_KEY | SUB_CLD);

    int32_t next_rrn;

    if (key < sub.key) {
        next_rrn = sub.left;
    } else if (key > sub.key) {
        next_rrn = sub.right;
    } else {
        // A chave já existe na árvore: atualizamos o offset em
        // vez de inserir uma nova chave ou retornar um erro
        //
        // SYNC: b_subnode
        sub.offset = offset;

        b_tree_put_subnode(page, ins_index, &sub, SUB_KEY);
        b_tree_write_page_or_mark_dirty(tree, page_rrn, page);

        return false;
    }

    bool was_promoted = b_tree_insert_impl(tree, next_rrn, key, offset, promoted);

    if (!was_promoted)
        return false;

    // Devemos fazer mais uma inserção, devido à promoção na volta da recursão.
    //
    // Como já não precisamos de `sub` (uma vez que a inserção no nó folha já foi
    // feita --- estamos na volta da recursão), copiamos `*promoted`, o nó que foi
    // promovido de um nível inferior, para `sub` antes de chamar
    // `b_tree_perform_insert` (que já irá inserir `sub` na posição desejada e
    // armazenar o nó promovido resultante dessa inserção em `*promoted`, se houver)
    memcpy(&sub, promoted, sizeof sub);

    return b_tree_perform_insert(tree, page_rrn, page, ins_index, &sub, promoted);
}

void b_tree_insert(b_tree_index_t *tree, uint32_t key, uint64_t offset)
{
    if (tree->root_rrn == -1)
        tree->root_rrn = b_tree_new_page(tree);

    b_tree_subnode_t promoted;
    bool was_promoted = b_tree_insert_impl(tree, tree->root_rrn, key, offset, &promoted);

    // Lida com o caso em que houve split e promoção no nó raiz.
    // 
    // Nesse caso, o novo nó raiz é inicialmente vazio, logo,
    // inserimos ordenadamente (de forma trivial)
    if (was_promoted) {
        tree->root_rrn = b_tree_new_page(tree);
        b_tree_init_page(&tree->root);

        *NODE_TYPE_P(&tree->root) = NODE_TYPE_ROOT;

        b_tree_shift_insert_subnode(&tree->root, 0, &promoted);
        tree->root_dirty = true;
    }
}

/**
 * Guarda parâmetros para a remoção que são os mesmos para todas as instâncias recursivas.
 *
 * - `found` deve ser inicializado com `false`;
 * - `key` deve ser inicializado com a chave a ser removida;
 * - `swap` deve ser inicializado com `SUBNODE_INIT`. 
 */
typedef struct {
    bool found;
    uint32_t key;
    b_tree_subnode_t swap;
} b_tree_remove_params_t;

/**
 * Remove o subnó com índice `index` do nó contido na página `page`, deslocando os demais
 * subnós para a esquerda e atualizando o tamanho. Preserva o filho direito do nó que vier
 * antes de `index` (se `index == 0`, preserva o filho esquerdo).
 */
static void b_tree_shift_remove_subnode(b_tree_page_t *page, uint32_t index)
{
    char *base = NODE_DATA_P(page) + SIZE_LEFT + SUBNODE_SKIP * index;

    char *src = base + SUBNODE_SKIP;
    char *dest = base;

    *NODE_LEN_P(page) -= 1;

    size_t len = *NODE_LEN_P(page) > 0
                     ? SUBNODE_SKIP * (*NODE_LEN_P(page) - index)
                     : 0;

    char *end = mempmove(dest, src, len);
    memset(end, -1, SUBNODE_SKIP);
}

/**
 * Tenta realizar uma redistribuição de chaves entre as páginas `left`, `right`
 * e `parent`, de modo que `parent` contém no índice `parent_search_index` o
 * subnó pai de `left` e `right`. `del_index` deve corresponder ao índice onde
 * o subnó a ser removido estará presente, no intervalo
 * [`0`, `*NODE_LEN_P(left) + *NODE_LEN_P(right)`) (ignorando a página pai).
 *
 * Retorna `true` se a redistribuição foi feita, removendo, para tal, a chave com
 * índice `del_index`, movendo os subnós entre as três páginas, conforme necessário,
 * e escrevendo as alterações realizadas nas páginas `left` e `right` de volta
 * no arquivo de dados (a página `parent` não é escrita no arquivo de dados).
 *
 * Retorna `false` se não for possível redistribuir, não alterando,
 * nesse caso, o conteúdo das páginas.
 *
 * NOTE: essa função recebe um ponteiro para a página pai, `parent`, para que seja
 * possível atualizá-la mediante a redistribuição e lidar com o caso em que, durante
 * a remoção, houve troca com o subnó de `parent` envolvido na redistribuição
 */
static bool b_tree_try_redistribute(b_tree_index_t *const tree,
                                    b_tree_page_t *left, b_tree_page_t *right,
                                    b_tree_page_t *parent, uint32_t del_index,
                                    uint32_t parent_search_index,
                                    b_tree_remove_params_t *params)
{
    // Tamanho das páginas `left` e `right` antes da redistribuição ocorrer de fato
    int32_t len_left = *NODE_LEN_P(left);
    int32_t len_right = *NODE_LEN_P(right);

    int32_t len_left_a = len_left;
    int32_t len_right_a = len_right;

    // Por definição da árvore B, os nós `left` e `right` têm o mesmo tipo
    enum b_node_type type = *NODE_TYPE_P(left);

    // NOTE: não faz sentido chamar essa função para o nó raiz; ignoramos esse caso
    size_t min_occup = type == NODE_TYPE_INTM
                                    ? MIN_OCCUPANCY_INTM
                                    : MIN_OCCUPANCY_LEAF;

    // Não será possível fazer a redistribuição
    if (len_left + len_right - 1 < 2 * min_occup)
        return false;

    // A diferença de tamanho entre os nós é usada para determinar a quantidade
    // de subnós que devem ser copiados.
    uint32_t len_diff = ABS(len_left - len_right);
    
    // `n` é a quantidade de subnós que devem ser copiados, ignorando o subnó `sub`.
    // 
    // No entanto, `n` é arredondado para baixo. `f` é usado para corrigir o valor
    // de `n` de modo que, após a redistribuição, o tamanho da página à esquerda
    // seja igual ao tamanho da página da direita (ou tenha um subnó a mais, se
    // a soma dos tamanhos for ímpar).
    int32_t n = len_diff / 2;
    int32_t f = len_diff % 2;

    char *src, *dest;
    enum which_skip skip;

    // Calcula os parâmetros para copiar os subnós, lembrando que devemos
    // copiar n subnós do final de `left` ou n subnós do começo de `right`,
    // dependendo do lado que tiver mais subnós.
    // 
    // Essa lógica, baseada no índice de remoção `del_index`, determina
    // o tipo de cópia que será feita, havendo quatro possibilidades:
    //
    //  - cópia de n subnós, sem pular nenhum subnó
    //    (remoção em `left` antes da região envolvida na cópia)
    //
    //  - cópia de (n - 1) subnós, pulando um subnó na posição `del_index` na fonte
    //    (remoção em `left` dentro da região envolvida na cópia)
    //
    //  - cópia de (n - 1) subnós, pulando um subnó na posição `del_index` no destino
    //    (remoção em `right` dentro da região envolvida na cópia)
    //
    //  - cópia de n subnós, sem pular nenhum subnó
    //    (remoção em `right`) após a região envolvida na cópia
    //
    // No caso da remoção fora da região envolvida na cópia, a remoção
    // já é feita antes da cópia.
    // 
    // Essa lógica só se aplica quando o nó da esquerda tem mais chaves,
    // pois considera `left` como fonte e `right` como destino, logo, o
    // valor de `skip` será invertido se o nó da direita tiver mais chaves.
    //
    // NOTE: o objetivo dessa implementação é ser genérica, funcionando
    // para qualquer tamanho de página. Uma implementação especializada
    // para o caso de uma árvore de ordem 2 teria que considerar menos
    // casos.
    if (del_index < len_left - n) {
        skip = SKIP_NONE;
        b_tree_shift_remove_subnode(left, del_index);

        len_left--;
        len_left_a--;
    } else if (del_index < len_left) {
        skip = SKIP_SRC;

        len_left_a--;
    } else {
        del_index -= len_left;

        if (del_index < n) {
            skip = SKIP_DEST;

            len_right_a--;
        } else {
            skip = SKIP_NONE;
            b_tree_shift_remove_subnode(right, del_index);

            len_right--;
            len_right_a--;
        }
    }

    b_tree_subnode_t sub;
    b_tree_get_subnode(parent, parent_search_index, &sub, SUB_KEY);

    // Se a chave advinda do nó pai (`sub.key`) for igual à chave de busca e
    // o valor de `swap` for válido, isso significa que houve troca, e o subnó
    // obtido do pai é justamente esse subnó trocado; devemos finalizar a troca
    //
    // SYNC: b_subnode
    if (sub.key == params->key && params->swap.left != -1) {
        sub.key = params->swap.key;
        sub.offset = params->swap.offset;
    }

    char *end;

    len_left = len_left_a;
    len_right = len_right_a;

    len_diff = ABS(len_left - len_right);

    n = len_diff / 2;
    f = len_diff % 2;

    if (len_left_a > len_right_a + 1) {
        if (n == 0)
            n += f;
        else
            n -= f;

        assert(n > 0);

        // Copiaremos `n - 1` subnós de `left` para `right`, logo,
        // começamos na posição `len_left - (n - 1) == len_left - n + 1`
        src = NODE_DATA_P(left) + SUBNODE_SKIP * (len_left - n + 1);
        dest = NODE_DATA_P(right);

        // Abrimos espaço para `n` subnós, pois o nó de destino
        // receberá n - 1 subnós de `left` e o outro será `sub`
        size_t len = SIZE_LEFT + SUBNODE_SKIP * len_right;
        memmove(dest + SUBNODE_SKIP * n, dest, len);

        *NODE_LEN_P(left) = len_left - n;
        *NODE_LEN_P(right) = len_right + n;
        
        end = b_tree_copy_subnodes_skipping_over(dest, src, n - 1, del_index, skip);

        //b_tree_get_subnode(left, len_left - n, &sub, SUB_R);

        // Copia a chave advinda da página pai para o final da região
        // da página da direita que estava envolvida na cópia
        b_tree_put_subnode(right, n - 1, &sub, SUB_KEY /*| SUB_R*/);

        // Prepara para copiar a chave de `left` para a página pai
        b_tree_get_subnode(left, len_left - n, &sub, SUB_KEY);

        // Após realizar a cópia, inicializa a fonte para o valor padrão (-1)
        memset(src - SUBNODE_SKIP, -1, n * SUBNODE_SKIP);
    } else {
        n += f;

        src = NODE_DATA_P(right);
        // O subnó em `sub` será inserido na posição `len_left`
        dest = NODE_DATA_P(left) + SUBNODE_SKIP * (len_left + 1);

        // Inverte a lógica acima, um pouco "cursed"
        if (skip != SKIP_NONE)
            skip = !skip;

        *NODE_LEN_P(left) = len_left + n;
        *NODE_LEN_P(right) = len_right - n;
        
        end = b_tree_copy_subnodes_skipping_over(dest, src, n - 1, del_index, skip);

        //b_tree_get_subnode(right, n - 1, &sub, SUB_L);

        // Copia a chave advinda da página pai para o começo da região
        // da página da esquerda que estava envolvida na cópia
        b_tree_put_subnode(left, len_left, &sub, SUB_KEY /*| SUB_L*/);

        // Prepara para copiar a chave de `right` para a página pai
        b_tree_get_subnode(right, n - 1, &sub, SUB_KEY);

        size_t len = SIZE_LEFT + SUBNODE_SKIP * (len_right - n);
        memmove(src, src + n * SUBNODE_SKIP, len);

        // Após realizar a cópia, inicializa a fonte para o valor padrão (-1)
        memset(end + SUBNODE_SKIP, -1, n * SUBNODE_SKIP);
    }

    b_tree_put_subnode(parent, parent_search_index, &sub, SUB_KEY);

    b_tree_get_subnode(parent, parent_search_index, &sub, SUB_CLD);

    b_tree_write_page(tree, sub.left, left);
    b_tree_write_page(tree, sub.right, right);
    
    return true;
}

/**
 * Concatena a página `right` a `left`, destruindo a página `right`,
 * rebaixando (inserindo) o subnó `demoted` e removendo o subnó no
 * índice `del_index`.
 */
static void b_tree_concat(b_tree_index_t *const tree,
                          int32_t left_rrn, b_tree_page_t *left,
                          int32_t right_rrn, b_tree_page_t *right,
                          uint32_t del_index, b_tree_subnode_t *demoted)
{
    size_t len_left = *NODE_LEN_P(left);
    size_t len_right = *NODE_LEN_P(right);

    // (+ 1 - 1) devido ao subnó "rebaixado" e ao subnó removido 
    assert(len_left + len_right <= N_KEYS);

    char *const src = NODE_DATA_P(right);
    char *const dest = NODE_DATA_P(left) + SUBNODE_SKIP * len_left;

    uint32_t n = len_right;

    if (del_index < len_left) {
        // Remove o subnó na posição `del_index`, deslocando os demais para a esquerda
        b_tree_shift_remove_subnode(left, del_index);

        // Insere o subnó "rebaixado" no fim (não atualiza o tamanho)
        //
        // NOTE: nesse caso, o fim corresponde a `len_left - 1`, pois
        // esse é o valor antigo (antes da remoção logo acima)
        b_tree_put_subnode(left, len_left - 1, demoted, SUB_KEY);

        // Copia a página da direita para o fim da página da esquerda
        b_tree_copy_subnodes(dest, src, n);
    } else {
        del_index -= len_left;

        // Insere o subnó "rebaixado" no fim (não atualiza o tamanho)
        b_tree_put_subnode(left, len_left, demoted, SUB_KEY);

        // Copia a página da direita para o fim da página da esquerda,
        // pulando o subnó na posição `del_index` na página da direita
        // (o subnó a ser removido). `SUBNODE_SKIP` é adicionado a
        // `dest` para pular o subnó que vai ser inserido em `len_left`
        b_tree_copy_subnodes_skipping_over(dest + SUBNODE_SKIP, src, n, del_index, SKIP_SRC);
    }

    // (+ 1 - 1) devido ao subnó "rebaixado" e ao subnó removido
    *NODE_LEN_P(left) = len_left + len_right;
    
    b_tree_write_page(tree, left_rrn, left);

    // INFO: (decisão de projeto/não especificado)
    //
    // Páginas que se tornaram vazias após a remoção/concatenação são
    // inicializadas antes de serem escritas de volta no arquivo de dados,
    // visando facilitar a depuração do código
    b_tree_init_page(right);
    b_tree_write_page(tree, right_rrn, right);

    tree->n_pages--;
}

enum del_status {
    REMOVED_DIRECT,
    REMOVED_REDIST,
    REMOVED_CONCAT_L,
    REMOVED_CONCAT_R
};

/**
 * Realiza a remoção do subnó com índice `del_index` no nó contido na página `page`, cujo RRN
 * deve ser `page_rrn`. `parent` deve ser a página pai de `page`, de modo que `parent_search_index`
 * corresponda ao índice em `parent` do subnó cujo filho esquerdo é `page`, exceto se `page` for o
 * último filho; nesse caso, poderá corresponder ao filho direito.
 *
 * Se possível, remove diretamente, realizando apenas um deslocamento, retornando `REMOVED_DIRECT`.
 * Senão, tenta realizar uma redistribuição entre `page` e a página irmã à direita, retornando
 * `REMOVED_REDIST`, ou concatenação, retornando `REMOVED_CONCAT_R`. Se não houver página irmã à
 * direita, faz uma concatenação com a página da esquerda, retornando `REMOVED_CONCAT_L`.
 *
 * Nos casos em que houver concatenação, é necessário remover o nó "despromovido"/"rebaixado"
 * de `parent`.
 */
static enum del_status b_tree_perform_remove(b_tree_index_t *const tree, int32_t page_rrn,
                                             b_tree_page_t *page, uint32_t del_index,
                                             b_tree_page_t *parent, uint32_t parent_search_index,
                                             b_tree_remove_params_t *params)
{
    size_t len = *NODE_LEN_P(page);
    enum b_node_type type = *NODE_TYPE_P(page);

    bool can_remove_directly = page == &tree->root
                                   || (type == NODE_TYPE_INTM && len > MIN_OCCUPANCY_INTM)
                                   || (type == NODE_TYPE_LEAF && len > MIN_OCCUPANCY_LEAF);

    // Remove diretamente (realizando deslocamento apenas) se a taxa de
    // ocupação após a remoção continuar acima da taxa de ocupação mínima
    if (can_remove_directly) {
        b_tree_shift_remove_subnode(page, del_index);

        // NOTE: a função `b_tree_shift_remove_subnode` não remove o último filho,
        // porém no único caso onde seria desejável (esperado) que o último filho
        // fosse removido (o caso em que há apenas um nó na árvore, que é raiz e
        // folha), o fato do nó ser folha implica que o último filho já possui o
        // valor que deveria ter (-1).
        //
        // Em todos os outros casos, nunca haverá remoção direta de um nó com
        // apenas um filho (haverá apenas redistribuição ou concatenação)

        b_tree_write_page_or_mark_dirty(tree, page_rrn, page);
        
        return REMOVED_DIRECT;
    }

    b_tree_subnode_t demoted;
    b_tree_get_subnode(parent, parent_search_index, &demoted, SUB_KEY | SUB_CLD);

    // SYNC: b_subnode
    if (demoted.key == params->key && params->swap.left != -1) {
        demoted.key = params->swap.left;
        demoted.offset = params->swap.offset;
    }

    if (demoted.left == page_rrn) {
        b_tree_page_t right;
        b_tree_read_page(tree, demoted.right, &right);

        if (b_tree_try_redistribute(tree, page, &right, parent, del_index, parent_search_index, params))
            return REMOVED_REDIST;

        // Sempre haverá espaço para concatenar, pois a concatenação só é feita se
        // não é possível redistribuir (ou seja, a soma das quantidades de chaves
        // entre `page` e sua página irmã é menor que o dobro da taxa de ocupação
        // mínima). Sendo assim, na primeira oportunidade, a concatenação será
        // realizada, e será obtido um nó cheio, devido à inserção de `demoted`.
        b_tree_concat(tree, page_rrn, page, demoted.right, &right, del_index, &demoted);

        return REMOVED_CONCAT_R;
    } else if (demoted.right == page_rrn) {
        b_tree_page_t left;
        b_tree_read_page(tree, demoted.left, &left);

        del_index += *NODE_LEN_P(&left);

        if (b_tree_try_redistribute(tree, &left, page, parent, del_index, parent_search_index, params))
            return REMOVED_REDIST;
        
        b_tree_concat(tree, demoted.left, &left, page_rrn, page, del_index, &demoted);

        return REMOVED_CONCAT_L;
    }

    __builtin_unreachable();
}

/**
 * Realiza a remoção de uma chave da árvore B recursivamente.
 *
 * `page_rrn` corresponde ao RRN da página contendo o nó atual na recursão,
 * `parent` deve ser um ponteiro para a página pai do nó atual, de modo que
 * o subnó no índice `parent_search_index` da página `parent` tenha como
 * filho esquerdo `page_rrn`, exceto se for o último subnó (filho direito).
 * `parent` e `parent_search_index` não precisam admitir um valor válido na
 * chamada da primeira instância recursiva.
 *
 * `params->swap` irá "trabalhar em dose dupla": na ida da recursão, guarda o
 * subnó a ser removido, a partir do momento em que esse subnó é encontrado em
 * um nó não-folha, para permitir que esse subnó seja trocado com seu sucessor
 * imediato na árvore. Na volta, passa a guardar o subnó com quem o subnó a
 * ser removido foi trocado, para que esse subnó possa ser inserido na árvore,
 * na posição adequada. Deve ser inicializado de modo que `swap->left` admita
 * um valor inválido (ex. `-1`) antes de chamar a primeira instância recursiva
 * dessa função.
 *
 * `params->found` deve ser inicializado com `false` antes da chamada da primeira
 * instância recursiva. Será atribuído o valor `true` se a chave buscada para
 * remoção for encontrada na árvore.
 *
 * Retorna o mesmo status de remoção descrito em `b_tree_perform_remove`.
 */
static enum del_status b_tree_remove_impl(b_tree_index_t *const tree, int32_t page_rrn,
                                          b_tree_page_t *parent, uint32_t parent_search_index,
                                          b_tree_remove_params_t *params)
{
    b_tree_page_t *page = b_tree_adequate_page(tree, page_rrn, &(b_tree_page_t){});

    size_t len = *NODE_LEN_P(page);

    // Esse caso só irá acontecer se a árvore estiver vazia, no entanto,
    // deve ser tratado
    if (len == 0)
        return false;

    b_tree_subnode_t sub;
    // Índice que seria removido, se o nó atual contivesse o subnó a ser removido
    // 
    // NOTE: da forma que foi implementada a busca binária, não é necessário
    // adicionar lógica para buscar pelo sucessor imediato na árvore, pois o
    // índice retornado pela busca binária (o índice onde um subnó com chave
    // `key` deveria ser inserido) nos guiará até o sucessor automaticamente
    uint32_t del_index = b_tree_bin_search(page, params->key, &sub);

    if (*NODE_TYPE_P(page) == NODE_TYPE_LEAF) {
        // A chave buscada não foi encontrada na árvore
        if (del_index == len)
            return false;

        params->found = true;

        // Se haverá troca na volta da recursão (o subnó sendo removido
        // não é o que queríamos remover), guarda o subnó a ser trocado
        // em `swap` (apenas os campos `key` e `offset`, pois queremos
        // preservar os filhos originais)
        if (params->key != sub.key) {
            // A chave buscada não foi encontrada no nó folha, e sabemos
            // que não estamos no caso em que a chave deveria ser trocada
            // com sua sucessora imediata na árvore, pois `swap->left`
            // contém o valor inválido com o qual foi inicializado, em vez
            // de um RRN válido.
            //
            // NOTE: o efeito de remover diretamente e não realizar uma
            // remoção é o mesmo para quem chamou essa função
            if (params->swap.left == -1)
                return REMOVED_DIRECT;

            // Preservamos os valores de `params->swap.left` e `params->swap.right`,
            // pois esses valores, obtidos de um nó intermediário (diferentes de -1),
            // indicam que houve uma troca
            // 
            // SYNC: b_subnode
            params->swap.key = sub.key;
            params->swap.offset = sub.offset;
        }
   
        return b_tree_perform_remove(tree, page_rrn, page, del_index, parent, parent_search_index, params);
    }

    // O subnó que queremos remover foi encontrado em um nó não-folha,
    // guardamos-o em `params->swap`
    if (del_index < len && params->key == sub.key)
        memcpy(&params->swap, &sub, sizeof sub);

    if (del_index == len) {
        del_index--;
        b_tree_get_subnode(page, del_index, &sub, SUB_KEY | SUB_CLD);
    }

    int32_t next_rrn;

    // Em ambos os casos do `else` (chave de busca foi encontrada no nó atual ou
    // chave de busca é maior que o nó atual), devemos seguir pela direita. No
    // primeiro caso, isso nos permitirá chegar ao sucessor do subnó contendo
    // essa chave na árvore.
    if (params->key < sub.key)
        next_rrn = sub.left;
    else
        next_rrn = sub.right;
   
    enum del_status status = b_tree_remove_impl(tree, next_rrn, page, del_index, params);

    // Estamos na volta da recursão, em um nó não-folha onde a chave
    // a ser removida foi encontrada, devemos realizar a troca antes
    // de retornar na recursão ou prosseguir com a possível remoção
    // decorrente de um rebaixamento
    //
    // NOTE: se a remoção foi realizada por meio de redistribuição,
    // o código de redistribuição já concluiu a troca antes de
    // redistribuir
    if (status != REMOVED_REDIST && params->key == sub.key) {
        b_tree_put_subnode(page, del_index, &params->swap, SUB_KEY);

        // Devemos fazer essa verificação aqui para evitar escrever a mesma
        // página da árvore B no disco várias vezes, desnecessariamente
        if (status == REMOVED_DIRECT)
            b_tree_write_page_or_mark_dirty(tree, page_rrn, page);
    }

    if (status == REMOVED_REDIST)
        b_tree_write_page_or_mark_dirty(tree, page_rrn, page);

    if (status == REMOVED_REDIST || status == REMOVED_DIRECT)
        return REMOVED_DIRECT;

    // Faríamos uma remoção na raiz que a tornaria vazia, devemos estabelecer o filho esquerdo
    // (que permaneceu após a concatenação) como novo nó raiz, diminuindo a altura da árvore
    //
    // NOTE: aqui realmente queremos comparar com `NODE_TYPE_ROOT`, pois esse código só deve
    // ser executado se o nó raiz não é uma folha (se for uma folha, devemos apenas realizar
    // a remoção, obtendo uma árvore possivelmente vazia, porém ao mesmo tempo preservando o
    // RRN do nó raiz --- embora a semântica esperada para essa parte do trabalho também não
    // tenha sido especificada, essa escolha parece sensata).
    if (*NODE_TYPE_P(page) == NODE_TYPE_ROOT && len == 1) {
        b_tree_get_subnode(&tree->root, 0, &sub, SUB_CLD);

        b_tree_init_page(&tree->root);
        b_tree_write_page(tree, tree->root_rrn, &tree->root);

        tree->n_pages--;
        
        tree->root_rrn = sub.left;
        
        b_tree_read_page(tree, tree->root_rrn, &tree->root);

        // Se o tipo do filho do antigo nó raiz era `NODE_TYPE_LEAF`,
        // o novo nó raiz deve continuar tendo o tipo `NODE_TYPE_LEAF`
        if (*NODE_TYPE_P(&tree->root) != NODE_TYPE_LEAF)
            *NODE_TYPE_P(&tree->root) = NODE_TYPE_ROOT;

        tree->root_dirty = true;

        return REMOVED_DIRECT;
    }

    return b_tree_perform_remove(tree, page_rrn, page, del_index, parent, parent_search_index, params);
}

bool b_tree_remove(b_tree_index_t *tree, uint32_t key)
{
    b_tree_remove_params_t params = {
        .found = false,
        .key = key,
        .swap = SUBNODE_INIT
    };

    b_tree_remove_impl(tree, tree->root_rrn, NULL, -1, &params);

    return params.found;
}

static bool b_tree_traverse_impl(b_tree_index_t *tree, uint32_t page_rrn, b_traverse_cb_t *cb, void *data)
{
    if (page_rrn == -1)
        return true;

    b_tree_page_t *page = b_tree_adequate_page(tree, page_rrn, &(b_tree_page_t){});

    b_tree_subnode_t sub;
    uint32_t len = *NODE_LEN_P(page);

    for (uint32_t i = 0; i < len; i++) {
        b_tree_get_subnode(page, i, &sub, SUB_L | SUB_KEY);

        if (!b_tree_traverse_impl(tree, sub.left, cb, data))
            return false;

        if (!cb(sub.key, sub.offset, data))
            return false;
    }

    b_tree_get_subnode(page, len - 1, &sub, SUB_R);

    return b_tree_traverse_impl(tree, sub.right, cb, data);
}

void b_tree_traverse(b_tree_index_t *tree, b_traverse_cb_t *cb, void *data)
{
    b_tree_traverse_impl(tree, tree->root_rrn, cb, data);
}

void b_tree_add_hook(b_tree_index_t *tree, b_hook_cb_t *cb, enum b_hook_type type, void *data)
{
    b_hook_t *hook = malloc(sizeof *hook);

    hook->cb = cb;
    hook->type = type;
    hook->data = data;
    hook->next = tree->hooks;
    
    tree->hooks = hook;
}
