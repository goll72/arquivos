#ifndef ERROR_H
#define ERROR_H

/** Definição de mensagens de erro */

/**
 * Mensagem impressa quando uma busca for solicitada
 * e não for encontrado nenhum registro válido antendendo
 * aos critérios de busca.
 */
#define E_NOREC "Registro inexistente."

/**
 * Mensagem impressa quando houver qualquer inconsistência
 * no arquivo ou erro ao realizar alguma operação solicitada.
 */
#define E_PROCESSINGFILE "Falha no processamento do arquivo."

#endif /* ERROR_H */
