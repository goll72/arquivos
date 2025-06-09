 - [x] Ler o resto do registro apenas se não estiver removido,
       ignorando o fato de que pode estar inválido. Documentar na
       função de leitura do registro que o resto do registro só será
       válido se o campo `removed` for `REC_NOT_REMOVED`

 - [x] Implementar a funcionalidade FUNC_SELECT_WHERE
       registro-a-registro, reutilizando código da funcionalidade
       FUNC_SELECT_STAR.

 - [x] Parar a busca antecipadamente se o campo buscado for `attack_id`
       e um registro com aquele ID já foi encontrado.

 - [x] Consertar `make -j` (dependência explícita no `$(BUILD)/.gitignore`),
       remover caracteres `/` ao final de `$(BUILD)`
       (`-j` não funciona para `make test`, no entanto)

 - [x] Usar apenas `NULO` para representar valores nulos no código de
       parsing de strings

       > A entrada `NULO` é válida para campos de tamanho fixo ou apenas
       > campos de tamanho variável?

 - [x] `parse.c`: adicionar comentários

 - [ ] Usar length-prefixed string `str_t` em vez de `char *` para campos de
       tamanho variável

 - [x] Mover `file_search_seq_next` para `file.c`

 - [ ] Criar um arquivo `crud.c` com as operações insert/delete/update (in-place)

 - [ ] Explicar melhor vsets

 - [ ] Explicar melhor X macros (no código)

 - [ ] Explicar melhor parsing

 - [x] Demarcar/anotar trechos de código que dependam da ordem dos campos
       de um registro com `SYNC: <tag>`, explicar essa tag

 - [x] Documentar a função `file_cleanup_after_modify`

 - [ ] Perguntar se os campos do nó da árvore B devem seguir os nomes da especificação

 - [x] ~~`top` -> `head` (lista de registros logicamente removidos)~~
