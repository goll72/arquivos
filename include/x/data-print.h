/**
 * Define os campos de dados do registro de dados, na
 * ordem em que devem ser impressos. A macro `DATA_FIELD`
 * pode ser usada para manipular esses campos, nessa ordem.
 */

#ifndef DATA_FIELD
#define DATA_FIELD(...)
#endif

DATA_FIELD(attack_id)      
DATA_FIELD(year)           
DATA_FIELD(country)        
DATA_FIELD(target_industry)
DATA_FIELD(attack_type)    
DATA_FIELD(financial_loss) 
DATA_FIELD(defense_mechanism)

#undef DATA_FIELD
