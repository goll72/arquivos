#ifndef TYPEFLAGS_H
#define TYPEFLAGS_H

/**
 * Flags representam atributos de um campo que podem se
 * aplicar a campos de tipos diferentes.
 *
 * NOTE: esse arquivo é separado do arquivo `typeinfo.h`,
 * uma vez que as flags definidas aqui precisam ser usados
 * em arquivos de cabeçalho referentes a X macros, que podem
 * ser inclusos em diversos contextos, não apenas no escopo global.
 *
 * Portanto, apenas `#define`s podem ser usados nesse arquivo.
 */

/**
 * F_UNIQUE: indica que o valor de um campo é único,
 * ou seja, é possível assumir que esse valor não irá
 * se repetir.
 */ 
#define F_UNIQUE (1 << 0)

#endif /* TYPEFLAGS_H */
