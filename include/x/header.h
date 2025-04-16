/**
 * Define os campos do registro de cabeçalho, de forma
 * que um cliente (código C) possa definir a macro
 * `HEADER_FIELD` e incluir esse arquivo para manipular
 * os campos de forma arbitrária (escrever ou ler cada
 * campo em um arquivo, por exemplo)
 */

#ifndef HEADER_FIELD
/*
 * A macro `HEADER_FIELD` pode ser definida para operar,
 * de alguma forma, sobre os metadados de cada campo.
 *
 * HEADER_FIELD(T, name, default)
 *   - T: tipo do campo
 *   - name: nome do campo
 *   - default: valor padrão do campo
 */
#define HEADER_FIELD(T, name, default)
#endif

/** Assume o valor `STATUS_CONSISTENT` se o arquivo estiver consistente. */
HEADER_FIELD(uint8_t,  status,                 STATUS_INCONSISTENT)

/** Topo da pilha de registros logicamente removidos, -1 indica que não há topo. */
HEADER_FIELD(int64_t,  top,                     -1)

/**
 * Próximo byte offset disponível para inserção no arquivo (seu valor é 0
 * se não houverem registros de dados, caso contrário, indica a posição onde
 * o arquivo termina - EOF).
 */
HEADER_FIELD(uint64_t, next_byte_offset,         0)

/** Quantidade de registros válidos presentes no arquivo. */
HEADER_FIELD(uint32_t, n_valid_recs,             0)

/** Quantidade de registros logicamente removidos presentes no arquivo. */
HEADER_FIELD(uint32_t, n_removed_recs,           0)                                      

HEADER_FIELD(char[23], attack_id_desc,         "IDENTIFICADOR DO ATAQUE")                
HEADER_FIELD(char[27], year_desc,              "ANO EM QUE O ATAQUE OCORREU")            
HEADER_FIELD(char[28], financial_loss_desc,    "PREJUIZO CAUSADO PELO ATAQUE")           
HEADER_FIELD(uint8_t,  country_code,           '1')                                      
HEADER_FIELD(char[26], country_desc,           "PAIS ONDE OCORREU O ATAQUE")             
HEADER_FIELD(uint8_t,  attack_type_code,       '2')                                      
HEADER_FIELD(char[38], attack_type_desc,       "TIPO DE AMEACA A SEGURANCA CIBERNETICA") 
HEADER_FIELD(uint8_t,  target_industry_code,   '3')                                      
HEADER_FIELD(char[38], target_industry_desc,   "SETOR DA INDUSTRIA QUE SOFREU O ATAQUE") 
HEADER_FIELD(uint8_t,  defense_mechanism_code, '4')                                      
HEADER_FIELD(char[67], defense_mechanism_desc, "ESTRATEGIA DE DEFESA CIBERNETICA EMPREGADA PARA RESOLVER O PROBLEMA")

#undef HEADER_FIELD
