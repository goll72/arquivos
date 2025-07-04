#ifndef B_TREE_INDEX
#define B_TREE_INDEX

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * Uma árvore B, que será usada como índice para o campo
 * ID no arquivo de dados. Não permite repetição de chaves.
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
 *
 * Retorna `NULL` se ocorrer algum erro.
 */
b_tree_index_t *b_tree_open(const char *path, const char *mode);

/**
 * Fecha o arquivo de dados da árvore B e desaloca outros
 * dados alocados internamente, chamando, antes, os hooks
 * de tipo `B_HOOK_CLOSE`.
 */
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
 * Insere um registro cuja chave é `key` e cujo offset é dado por `offset`
 * na árvore B. A operação realizada é na verdade um "upsert": se a chave
 * `key` já existir na árvore, seu offset correspondente é atualizado; se
 * não existir, é inserida na árvore.
 */
void b_tree_insert(b_tree_index_t *tree, uint32_t key, uint64_t offset);

/**
 * Remove o registro cuja chave é `key` da árvore B.
 * Retorna `true` se o elemento existia na árvore e foi removido.
 */
bool b_tree_remove(b_tree_index_t *tree, uint32_t key);

typedef bool b_traverse_cb_t(uint32_t key, uint64_t offset, void *data);

/**
 * Percorre a árvore B em profundidade, ordenadamente, chamando para cada chave
 * a função `cb` com o parâmetro `data`.
 *
 * Se `cb` retornar `false`, o percurso será interrompido prematuramente.
 */
void b_tree_traverse(b_tree_index_t *tree, b_traverse_cb_t *cb, void *data);

typedef void b_hook_cb_t(FILE *, void *);

/**
 * Define tipos de hook para a árvore B (o único relevante para esse
 * projeto é `B_HOOK_CLOSE`)
 */
enum b_hook_type {
    B_HOOK_CLOSE
};

/** Adiciona um hook à árvore B. */
void b_tree_add_hook(b_tree_index_t *tree, b_hook_cb_t *cb, enum b_hook_type type, void *data);

#endif /* B_TREE_INDEX */
