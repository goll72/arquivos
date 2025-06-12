#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdalign.h>

#include "index/b_tree.h"
#include "defs.h"

#define FAIL_IF(x) \
    if (x)         \
        return false;

/**
 * Implementação de uma árvore B flexível quanto ao tamanho da página:
 * basta alterar o valor da macro `PAGE_SIZE` para usar um tamanho
 * diferente de página de disco.
 */

#define PAGE_SIZE   44
#define HEADER_SIZE PAGE_SIZE

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

static_assert(SIZE_LEFT == sizeof ((b_tree_subnode_t *)0)->left, "SIZE_LEFT");

/**
 * Quantidade de bytes a serem pulados para ir de um "subnó" para o
 * próximo em uma página. Não inclui o campo `left` devido à sobreposição
 * entre os campos `left` e `right` mencionada acima.
 */
#define SUBNODE_SKIP (4 /* sizeof key */ + 8 /* sizeof offset */ + 4 /* sizeof right */)

/** Quantidade de chaves que podem ser armazenadas em um nó/uma página da árvore */
#define N_KEYS ((PAGE_SIZE - SIZE_LEFT) / SUBNODE_SKIP)

/** Ordem da árvore */
#define TREE_ORDER (N_KEYS + 1)

/** Verdadeiro se houver espaço no final da página da árvore, que deverá ser preenchido com '$' */
#define TREE_PAGE_NEEDS_PADDING N_KEYS * SUBNODE_SKIP + SIZE_LEFT < PAGE_SIZE

/** Para esse trabalho, a ordem da árvore deve ser 3 */
static_assert(TREE_ORDER == 3, "TREE_ORDER");

/** Guarda uma página (nó) da árvore B */
typedef struct {
    alignas(uint32_t) char data[PAGE_SIZE];
} b_tree_page_t;

/** Ponteiro para o tipo do nó `p` */
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

    /** Status do arquivo de dados da árvore */
    uint8_t status;

    /** Conteúdo da página do nó raiz */
    b_tree_page_t root;
};

#define HEADER_FIELDS(X)  \
    X(uint8_t,  status)   \
    X(uint32_t, root_rrn) \
    X(uint32_t, next_rrn) \
    X(uint32_t, n_pages)

#define PACKED(T) packed_##T

#define X(T, name) T name;

typedef struct {
    HEADER_FIELDS(X)
} __attribute__((packed)) PACKED(b_header_t);

typedef struct {
    SUBNODE_FIELDS(X)
} __attribute__((packed)) PACKED(b_tree_subnode_t);

#undef X

static uint32_t b_tree_new_page(b_tree_index_t *tree)
{
    return tree->next_rrn++;
}

static void b_tree_init_page(b_tree_page_t *page)
{
    char *p = NODE_DATA_P(page);
    const char *end = &page->data[sizeof page->data];

    *NODE_TYPE_P(page) = NODE_TYPE_LEAF;
    *NODE_LEN_P(page) = 0;

    // Inicializa o campo `left`
    *p++ = -1;
    
    // Inicializa os demais campos (o valor padrão para todos é -1)
    while ((char *)p + SUBNODE_SKIP < end)
        *p++ =  -1;

#if TREE_PAGE_NEEDS_PADDING
    // Preenche o espaço que sobrar na página com '$'
    for (char *q = (char *)p; q != end; q++)
        *q = '$';
#endif
}

static bool b_tree_read_header(b_tree_index_t *tree)
{
    fseek(tree->file, 0L, SEEK_SET);

    #define X(_, name) FAIL_IF(fwrite(&tree->name, sizeof tree->name, 1, tree->file) != 1)

    HEADER_FIELDS(X)

    #undef X

    return true;
}

static bool b_tree_write_header(b_tree_index_t *tree)
{
    fseek(tree->file, 0L, SEEK_SET);

    #define X(_, name) FAIL_IF(fwrite(&tree->name, sizeof tree->name, 1, tree->file) != 1)

    HEADER_FIELDS(X)

    #undef X

    // XXX: escrever cifrões/lixo ('$')

    return true;
}

static void b_tree_read_page(b_tree_index_t *tree, uint32_t rrn, b_tree_page_t *page)
{
    fseek(tree->file, HEADER_SIZE + rrn * PAGE_SIZE, SEEK_SET);
    fread(page, PAGE_SIZE, 1, tree->file);
}

static void b_tree_write_page(b_tree_index_t *tree, uint32_t rrn, b_tree_page_t *page)
{
    fseek(tree->file, HEADER_SIZE + rrn * PAGE_SIZE, SEEK_SET);
    fwrite(page, PAGE_SIZE, 1, tree->file);
}

b_tree_index_t *b_tree_open(const char *path, const char *mode)
{
    FILE *file = fopen(path, mode);

    if (!file)
        return NULL;

    bool mode_is_modify = strchr(mode, 'w') || strchr(mode, '+');
    
    b_tree_index_t *tree = malloc(sizeof *tree);

    tree->file = file;
    tree->root_dirty = false;

    if (!b_tree_read_header(tree)) {
        // Assumimos que o arquivo está vazio e o inicializamos.
        // 
        // Se o arquivo não foi aberto de modo a permitir modificação
        // e não possui cabeçalho válido, isso é considerado um erro
        if (!mode_is_modify) {
            b_tree_close(tree);
            return NULL;
        }

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
    }

    if (mode_is_modify) {
        tree->status = B_STATUS_INCONSISTENT;

        fseek(tree->file, offsetof(PACKED(b_header_t), status), SEEK_SET);
        fwrite(&tree->status, sizeof tree->status, 1, tree->file);
    }

    return tree;
}

void b_tree_close(b_tree_index_t *tree)
{
    if (tree->root_dirty)
        b_tree_write_page(tree, tree->root_rrn, &tree->root);

    tree->status = B_STATUS_CONSISTENT;

    // XXX: tenta escrever mesmo se o arquivo foi aberto apenas para leitura
    fseek(tree->file, offsetof(PACKED(b_header_t), status), SEEK_SET);
    fwrite(&tree->status, sizeof tree->status, 1, tree->file);

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
 */
static uint32_t b_tree_bin_search(const b_tree_page_t *page, uint32_t key, b_tree_subnode_t *sub)
{
    uint32_t len = *(uint32_t *)NODE_LEN_P(page);
    uint32_t low = 0;
    uint32_t high = len;

    uint32_t prev = low + (high - low + 1) / 2;
    uint32_t mid = 0;
    
    b_tree_get_subnode(page, mid, sub);

    while (true) {
        if (key > sub->key)
            low = mid + 1;
        else if (key < sub->key)
            high = mid;
        else
            break;

        prev = mid;
        mid = low + (high - low + 1) / 2;

        if (prev == mid)
            break;

        b_tree_get_subnode(page, mid, sub);
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

    b_tree_subnode_t sub;
    b_tree_bin_search(page, key, &sub);

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
 * Realiza a operação "split" de uma página `page` da árvore B em duas,
 * redistribuindo as chaves uniformemente e promovendo uma chave. Deve
 * haver espaço para uma página da árvore B em `new`, `ins_index` deve
 * ser o índice onde será inserida uma chave.
 *
 * Guarda em `*promoted` os dados da chave que foi promovida.
 *
 * XXX: leave space at ins_index and take into account that it may be on
 * the right page rather than on the left
 */
int32_t b_tree_split_page(b_tree_index_t *tree, b_tree_page_t *page, b_tree_page_t *new, uint32_t ins_index, b_tree_subnode_t *promoted)
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
    // redistribuídas entre os dois nós
    static const uint32_t len_right = (N_KEYS - 1) / 2;
    static const uint32_t len_left = N_KEYS - 1 - len_right;

    b_tree_get_subnode(page, len_left, promoted);

    // Adicionamos 1 para que a chave a ser promovida não seja copiada
    char *src = NODE_DATA_P(page) + SUBNODE_SKIP * (len_left + 1);
    char *dest = NODE_DATA_P(new);

    size_t len = SIZE_LEFT + len_right * SUBNODE_SKIP;

    memcpy(dest, src, len);
    // Queremos apagar a chave promovida
    memset(src - SUBNODE_SKIP, -1, len + SUBNODE_SKIP);

    *NODE_LEN_P(page) = len_left;
    *NODE_LEN_P(new) = len_right;

    return new_rrn;
}

static void b_tree_shift_insert_subnode(b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub)
{
    // XXX: this can overflow
    char *base = NODE_DATA_P(page) + SUBNODE_SKIP * index;

    char *src = base;
    char *dest = base + SUBNODE_SKIP;
    const char *end = NODE_DATA_P(page) + SUBNODE_SKIP * *NODE_LEN_P(page);

    memmove(dest, src, end - dest);

    b_tree_put_subnode(page, index, sub);

    *NODE_LEN_P(page) += 1;
}

/** Implementa a inserção de fato, de forma recursiva; vd. `b_tree_insert`. */
static bool b_tree_insert_impl(b_tree_index_t *const tree, int32_t page_rrn, uint32_t key, uint64_t offset, b_tree_subnode_t *promoted)
{
    b_tree_page_t *page = b_tree_adequate_page(tree, page_rrn, &(b_tree_page_t){});

    if (*NODE_TYPE_P(page) == NODE_TYPE_LEAF) {
        b_tree_subnode_t sub;
        // Índice onde a inserção irá ocorrer, se houver espaço
        uint32_t ins_index = b_tree_bin_search(page, key, &sub);
        
        if (*NODE_LEN_P(page) < N_KEYS) {
            sub.left = -1;
            sub.key = key;
            sub.offset = offset;
            // << Queremos preservar o valor de sub.right

            // Insere ordenado
            b_tree_shift_insert_subnode(page, ins_index, &sub);

            b_tree_write_page(tree, page_rrn, page);

            return false;
        } else {
            b_tree_page_t new;

            // Split
            int32_t new_rrn = b_tree_split_page(tree, page, &new, ins_index, promoted);
            b_tree_put_subnode(page, ins_index, &sub);

            promoted->left = page_rrn;
            promoted->right = new_rrn;

            b_tree_write_page(tree, page_rrn, page);
            b_tree_write_page(tree, new_rrn, &new);

            return true;
        }
    }

    b_tree_subnode_t sub;
    // Índice onde a inserção deveria ocorrer, se esse nó fosse um nó folha
    uint32_t ins_index = b_tree_bin_search(page, key, &sub);

    int32_t next_rrn;

    if (key > sub.key)
        next_rrn = sub.right;
    else if (key < sub.key)
        next_rrn = sub.left;
    else
        // XXX: error handling
        return false;

    bool was_promoted = b_tree_insert_impl(tree, next_rrn, key, offset, promoted);

    if (was_promoted) {
        if (next_rrn == sub.right)
            ins_index = ins_index + 1;
        
        if (*NODE_LEN_P(page) < N_KEYS) {
            // Insere ordenado
            b_tree_shift_insert_subnode(page, ins_index, promoted);

            b_tree_write_page(tree, page_rrn, page);

            return false;
        } else {
            b_tree_page_t new;

            // Iremos realizar outra promoção, porém ainda temos que fazer o "split"
            // e então inserir o subnó que recebemos (que foi promovido de um nível inferior).
            //
            // Como já não precisamos de `sub` (originalmente usado para a inserção, no entanto
            // a inserção já foi feita --- estamos na volta da recursão), copiamos o nó promovido
            // de um nível inferior para `sub`, realizamos o "split" e então inserimos `sub` na
            // posição desejada. 
            memcpy(&sub, promoted, sizeof sub);

            // Split
            int32_t new_rrn = b_tree_split_page(tree, page, &new, ins_index, promoted);
            b_tree_put_subnode(page, ins_index, &sub);

            // XXX: idk if this is right atp
            promoted->left = page_rrn;
            promoted->right = new_rrn;

            b_tree_write_page(tree, page_rrn, page);
            b_tree_write_page(tree, new_rrn, &new);

            return true;
        }
    }

    return false;
}

void b_tree_insert(b_tree_index_t *tree, uint32_t key, uint64_t offset)
{
    if (tree->root_rrn == -1) {
        tree->root_dirty = true;
        tree->root_rrn = b_tree_new_page(tree);
    }

    b_tree_subnode_t promoted;
    b_tree_insert_impl(tree, tree->root_rrn, key, offset, &promoted);

    if (tree->root_dirty) {
        b_tree_write_page(tree, tree->root_rrn, &tree->root);
        tree->root_dirty = false;
    }
}
