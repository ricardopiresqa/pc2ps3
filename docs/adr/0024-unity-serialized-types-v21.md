# ADR-0024: Leitura limitada da tabela SerializedType v21

- Status: Accepted
- Data: 2026-11-03

## Contexto

O leitor já valida o cabeçalho, o metadata preamble e blobs TypeTree. O próximo
trecho do SerializedFile v21 descreve tipos antes da object table. Um deslocamento
incorreto nessa tabela pode fazer o parser consumir bytes da object table como
metadados, e um count não limitado pode causar alocação excessiva.

## Decisão

Implementar `parse_serialized_type_table` somente para formato 21 com TypeTree
habilitada. Ler `type_count` como int32 endian-aware, limitado a 10.000. Preservar
classID, `stripped` validado como 0/1, `scriptTypeIndex`, `oldTypeHash` de 16 bytes
e o TypeTree retornado pelo parser existente. Consumir `scriptID` de 16 bytes
somente para `classID` 114.

Ler `typeDependencies` como int32, limitando cada tipo a 100.000 e a tabela a
250.000. Limitar também o agregado a 200.000 TypeTree nodes e 4 MiB de string
buffers. Toda leitura passa pelo limite `metadata_end`. Retornar os tipos
completos e o cursor final; qualquer erro levanta `UnitySerializedFileError` e
não expõe uma tabela parcial.

## Consequências

A object table, os objetos e seus payloads continuam fora do escopo deste parser.
Offsets common-string continuam brutos, sem resolução de nomes. O leitor pode
avançar para a object table apenas a partir do cursor retornado por esta função.

Fixtures sintéticas cobrem os tipos comuns e `classID` 114, os dois endians,
TypeTree, dependências, limites e metadata truncado. A evidência local foi obtida
em memória no bundle UnityFS de um build Unity, sem exportar payloads.
