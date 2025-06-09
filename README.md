arquivos
========

Projeto final da disciplina de Organização de Arquivos. Consiste de um
proto-banco-de-dados com índice (árvore B).

Para compilar, usar e testar, use `make`, `make run`, `make test TESTS="..."`
(vd. `tests/*.in`), `make debug TEST=...`, etc.

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
