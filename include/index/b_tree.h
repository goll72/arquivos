#ifndef B_TREE_INDEX
#define B_TREE_INDEX

#include <stdint.h>
#include <stdbool.h>

/**
 * Uma árvore B, que será usada como índice para o campo
 * ID no arquivo de dados.
 */

typedef struct b_tree_index b_tree_index_t;

/**
 * Abre o arquivo de dados da árvore B cujo caminho é `path`,
 * usando a função `fopen` com modo de abertura dado em `mode`.
 * Também inicializa campos relevantes, verifica se o arquivo
 * é válido e marca o status como inconsistente, se o modo de
 * abertura permitir modificação.
 *
 * Se não houver cabeçalho e o modo de abertura permitir modificação,
 * assume que o arquivo está vazio e cria um registro de cabeçalho.
 */
b_tree_index_t *b_tree_open(const char *path, const char *mode);

/** Fecha o arquivo de dados da árvore B. */
void b_tree_close(b_tree_index_t *tree);

/**
 * Dada a chave de busca `key`, busca-a na árvore e, se for
 * encontrada, seu offset correspondente é guardado em `*offset`
 * e `true` é retornado.
 *
 * Se a chave não for encontrada, retorna `false`.
 */
bool b_tree_search(b_tree_index_t *tree, uint32_t key, uint64_t *offset);

/**
 * Insere um registro cuja chave é `key` e cujo offset é dado por
 * `offset` na árvore B.
 */
void b_tree_insert(b_tree_index_t *tree, uint32_t key, uint64_t offset);

#endif /* B_TREE_INDEX */
