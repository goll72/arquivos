#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <stdalign.h>

#include "index/b_tree.h"

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

static_assert(TREE_ORDER == 3);

/** Guarda uma página (nó) da árvore B */
typedef struct {
    alignas(uint32_t) char data[PAGE_SIZE];
} b_tree_page_t;

#define NODE_TYPE_P(p) ((uint32_t *)&(p)->data[0])
#define NODE_LEN_P(p)  ((uint32_t *)&(p)->data[4]) 
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
    uint32_t next;
    
    /** RRN da página que contém o nó raiz da árvore */
    uint32_t root_rrn;
    
    /**
     * Indica se a página do nó raiz contida em `root` está
     * "suja", ou seja, foi modificada e deve ser reescrita
     * no arquivo de dados.
     */
    bool root_dirty;

    /** Conteúdo da página do nó raiz */
    b_tree_page_t root;
};

#define HEADER_FIELDS(X)  \
    X(uint32_t, next)     \
    X(uint32_t, root_rrn)

static uint32_t b_tree_new_page(b_tree_index_t *tree)
{
    return tree->next++;
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

b_tree_index_t *b_tree_open(const char *path)
{
    // FIXME: modo de abertura
    FILE *file = fopen(path, "wb+");

    if (!file)
        return NULL;
    
    b_tree_index_t *tree = malloc(sizeof *tree);

    tree->file = file;
    tree->root_dirty = false;

    if (!b_tree_read_header(tree)) {
        // Assumimos que o arquivo está vazio e o inicializamos

        tree->next = 0;
        tree->root_rrn = -1;
        
        b_tree_init_page(&tree->root);

        b_tree_write_header(tree);
    } else {
        b_tree_read_page(tree, tree->root_rrn, &tree->root);
    }
    
    return tree;
}

void b_tree_close(b_tree_index_t *tree)
{
    if (tree->root_dirty)
        b_tree_write_page(tree, tree->root_rrn, &tree->root);
    
    fclose(tree->file);

    free(tree);
}

/**
 * Obtém o subnó da página `page` com
 * índice `index` e o armazena em `*sub`.
 */
static void b_tree_get_subnode(b_tree_page_t *page, uint32_t index, b_tree_subnode_t *sub)
{
    size_t base = sizeof(uint32_t) + SUBNODE_SKIP * index;
 
    #define X(T, name) \
        memcpy(&sub->name, &page->data[base + offsetof(b_tree_subnode_t, name)], sizeof sub->name);

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
static uint32_t b_tree_bin_search(b_tree_page_t *page, uint32_t key, b_tree_subnode_t *sub)
{
    uint32_t len = *(uint32_t *)page->data;
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

/** Implementa a inserção de fato, de forma recursiva. */
static void b_tree_insert_impl(int32_t page_rrn, uint32_t key, uint64_t offset)
{
    
}

void b_tree_insert(b_tree_index_t *tree, uint32_t key, uint64_t offset)
{
    if (tree->root_rrn == -1) {
        tree->root_dirty = true;
        tree->root_rrn = b_tree_new_page(tree);
    }

    // Chamada recursiva

    if (tree->root_dirty) {
        b_tree_write_page(tree, tree->root_rrn, &tree->root);
        tree->root_dirty = false;
    }
}
