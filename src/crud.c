#include "defs.h"
#include "file.h"
#include "vset.h"
#include "crud.h"

bool crud_insert(FILE *f, f_header_t *header, f_data_rec_t *rec)
{
    // Guarda o tamanho realmente ocupado pelo registro (em contraste
    // ao tamanho disponível para inserção em uma determinada posição,
    // devido ao algoritmo de reaproveitamento de espaço usando a lista
    // de registros removidos)
    uint64_t actual_size = rec->size;

    int64_t insert_off = header->top;

    // Usado para atualizar a lista de removidos após a inserção
    int64_t prev = -1;
    int64_t next = -1;

    // Procura pelo primeiro registro com espaço suficiente
    // na lista de registros logicamente removidos (algoritmo first-fit)
    while (insert_off != -1) {
        fseek(f, insert_off, SEEK_SET);

        uint8_t removed;

        // SYNC: rec
        if (fread(&removed, sizeof removed, 1, f) != 1)
            return false;

        if (removed != REC_REMOVED)
            return false;

        // SYNC: rec
        if (fread(&rec->size, sizeof rec->size, 1, f) != 1)
            return false;

        // SYNC: rec
        if (fread(&next, sizeof next, 1, f) != 1)
            return false;

        // Encontramos um registro com espaço suficiente
        if (rec->size >= actual_size)
            break;

        prev = insert_off;
        insert_off = next;
    }

    // Realiza a inserção no fim.
    //
    // NOTE: `insert_off` pode ser `-1` em dois casos:
    //
    // - a lista de removidos está vazia; ou
    // - não foi encontrado um registro com espaço suficiente na lista.
    //
    // Um desses casos entra em um laço que muda o valor de `rec->size`,
    // o outro não, logo, devemos atribuir o valor de `rec->size` novamente aqui.
    if (insert_off == -1) {
        insert_off = header->next_byte_offset;
        rec->size = actual_size;
    }

    fseek(f, insert_off, SEEK_SET);

    if (!file_write_data_rec(f, header, rec))
        return false;

    // Caso especial: um registro foi inserido na posição dada
    // pela cabeça, apenas a cabeça precisa ser atualizada
    if (insert_off == header->top) {
        header->top = next;
    } else {
        fseek(f, prev + offsetof(PACKED(f_data_rec_t), next_removed_rec), SEEK_SET);
        fwrite(&next, sizeof next, 1, f);
    }

    header->n_valid_recs++;

    // Se o registro foi inserido no fim, devemos atualizar o próximo byte offset disponível
    if (insert_off == header->next_byte_offset)
        header->next_byte_offset = ftell(f);
    else
        header->n_removed_recs--;

    return true;
}

bool crud_delete(FILE *f, f_header_t *header, f_data_rec_t *rec)
{
    long rec_off = ftell(f);
    
    // Marca o registro como removido e o insere na cabeça da lista de registros
    // logicamente removidos
    rec->removed = REC_REMOVED;
    rec->next_removed_rec = header->top;
    header->top = rec_off;

    // As modificações no registro de cabeçalho contido no arquivo são postergadas
    // para o momento em que o arquivo é fechado. Não há risco de corrupção
    // silenciosa do arquivo de dados, graças ao uso da flag de status.
    header->n_valid_recs--;
    header->n_removed_recs++;

    // Como devemos atualizar apenas os campos `removed` e `next_removed_rec`,
    // iremos apenas fazer as modificações diretamente, em vez de usar a função
    // `file_write_data_rec`, que escreve todo o registro.
    fseek(f, rec_off, SEEK_SET);

    // SYNC: rec
    fwrite(&rec->removed, sizeof rec->removed, 1, f);
    fseek(f, sizeof rec->size, SEEK_CUR);
    fwrite(&rec->next_removed_rec, sizeof rec->next_removed_rec, 1, f);

    return true;
}

bool crud_update(FILE *f, f_header_t *header, f_data_rec_t *rec, vset_t *patch)
{
    uint64_t old_size = rec->size;

    // Aplica as alterações contidas no vset `patch` em `rec`.
    vset_patch(patch, rec);

    rec->size = DATA_REC_SIZE_AFTER_SIZE_FIELD;

    #define VAR_FIELD(T, name, repr) rec->size += rec->name ? strlen(rec->name) + 2 : 0;

    // Calcula o tamanho do registro após realizar as modificações contidas em `patch`
    #include "x/data.h"

    // Faz a atualização in-place (na mesma posição), uma vez que o tamanho que será
    // ocupado pelo registro após a modificação é menor que o tamanho disponível
    //
    // NOTE: a função `file_write_data_rec` espera que o tamanho em `rec->size`
    // corresponda ao tamanho disponível naquela posição, enquanto `crud_insert`
    // espera que o tamanho em `rec->size` seja o tamanho realmente ocupado pelo
    // registro (ignorando o lixo), e ajusta o valor, se for necessário, para
    // chamar `file_write_data_rec`. Por isso realizamos a atribuição abaixo.
    if (rec->size < old_size) {
        rec->size = old_size;

        if (!file_write_data_rec(f, header, rec))
            return false;

        header->n_valid_recs++;

        return true;
    }

    f_data_rec_t tmp = {};

    return crud_delete(f, header, &tmp) && crud_insert(f, header, rec);
}
