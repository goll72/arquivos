arquivos
========

Projeto final da disciplina de Organização de Arquivos. Consiste de um
proto-banco-de-dados com índice (árvore B) capaz de armazenar apenas um
tipo de registro (dados sobre ataques cibernéticos). Por motivos de força
maior, a remoção com índice não foi implementada, embora a remoção na
árvore B tenha sido implementada.

Para compilar, usar e testar, use `make`, `make run`, `make test TESTS="..."`
(substitua `...` por um ou mais prefixos referentes aos arquivos de entrada/saída
dos casos de teste, ou omita a variável `TESTS` para rodar todos os casos de teste;
vd. `tests/*.in`), `make debug TEST=...` para depurar um caso de teste específico
com o `lldb` etc.

## Dependências

 - GNU make
 - Um compilador GNU C11 (clang/gcc)

Para desenvolvimento/teste:

 - hexdump
 - git
 - bubblewrap
 - lldb

## Scripts

Alguns scripts úteis podem ser encontrados em `scripts/`:

 - `diff.sh`: wrapper para o `git diff` para arquivos dentro/fora
   do repositório

 - `diff-trees.sh`: usa o `diff.sh` para comparar duas árvores de
   diretórios, porém com uma diferença para o diff usual: ao
   comparar `a` com `b`, ignora arquivos em `a` que não estão
   presentes com `b`. Usado junto com a sandbox bubblewrap e o
   `test.mk` para mostrar as diferenças entre as saídas obtidas
   e esperadas, bem como os arquivos gerados e esperados.

 - `dump_b_tree.c`: mostra o conteúdo de um arquivo de dados 
   da árvore B de uma forma fácil de visualizar, usando cores

 - `fuzz_b_tree.c`: usa o `libFuzzer` do LLVM para fazer fuzz
   testing da inserção na árvore B, visando encontrar bugs de
   uso de memória em casos patológicos (dica: use juntamente
   com o ASAN)

 - `compare-intermediate-trees.sh`: dados dois programas `prog1`
   e `prog2`, sendo um deles uma implementação "referência" e
   o outro uma implementação sendo depurada, compara as árvores
   intermediárias geradas após executar cada linha de um determinado
   caso de teste em ambos, de modo a permitir encontrar onde as
   implementações divergem.

## Observações

O target `all` (`make`) irá, por padrão, rodar:

```sh
git config include.path ../.gitconfig
```

Esteja ciente disso.
