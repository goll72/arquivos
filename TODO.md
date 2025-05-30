 - [ ] Ler o resto do registro apenas se não estiver removido,
       ignorando o fato de que pode estar inválido. Documentar na
       função de leitura do registro que o resto do registro só será
       válido se o campo `removed` for `REC_NOT_REMOVED`

 - [ ] Explicar melhor X macros (no código)

 - [ ] Implementar a funcionalidade FUNC_SELECT_WHERE
       registro-a-registro, reutilizando código da funcionalidade
       FUNC_SELECT_STAR.

 - [ ] Parar a busca antecipadamente se o campo buscado for `attack_id`
       e um registro com aquele ID já foi encontrado.

 - [x] Usar apenas `NULO` para representar valores nulos no código de
       parsing de strings

       > A entrada `NULO` é válida para campos de tamanho fixo ou apenas
       > campos de tamanho variável?

 - [x] `parse.c`: adicionar comentários
