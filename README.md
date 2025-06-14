arquivos
========

Projeto final da disciplina de Organização de Arquivos. Consiste de um
proto-banco-de-dados com índice (árvore B).

Para compilar, usar e testar, use `make`, `make run`, `make test TESTS="..."`
(substitua `...` por um ou mais prefixos referentes aos arquivos de entrada/saída
dos casos de teste, ou omita a variável `TESTS` para rodar todos os casos de teste;
vd. `tests/*.in`), `make debug TEST=...`, etc.

## Dependências

 - GNU make
 - Um compilador GNU C11 (clang/gcc)

Para desenvolvimento/teste:

 - hexdump
 - git
 - bubblewrap
 - lldb

## Observações

O target `all` (`make`) irá, por padrão, rodar:

```sh
git config include.path ../.gitconfig
```

Esteja ciente disso.
