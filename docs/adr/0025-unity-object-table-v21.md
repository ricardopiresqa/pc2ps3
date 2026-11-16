# ADR-0025: Leitura limitada da object table Unity SerializedFile v21

- Status: Accepted
- Data: 2026-11-03

## Contexto

Após a tabela SerializedType, o SerializedFile v21 associa `pathID`s a tipos e a
ranges de dados. A tabela fica dentro do metadata, mas seus `byteStart` apontam
para a área de dados. O leitor precisa validar ambos os limites sem confundir
bytes da script table ou payloads com registros de objeto.

## Decisão

Implementar `parse_serialized_object_table` apenas para v21 com TypeTree habilitada,
começando no cursor retornado pelo parser SerializedType. Limitar `object_count`
int32 a 250.000. Antes de cada registro, alinhar a 4 bytes; ler `pathID` int64,
`byteStart` uint32 relativo a `data_offset`, `byteSize` uint32 e `typeID` int32.
Resolver classID pela tabela SerializedType e rejeitar typeIDs fora dela.

Validar `[data_offset + byteStart, início + byteSize)` dentro de `file_size` antes
de devolver o registro. Leituras da object table passam por `metadata_end`. Retornar
objetos completos e cursor imediatamente antes da script table. Qualquer count,
range ou registro truncado levanta `UnitySerializedFileError`; não retornar lista
parcial nem ler payload.

## Consequências

A tabela de scripts e a leitura dos dados dos objetos ficam em parsers posteriores.
O campo `byteStart` permanece relativo e o classID é resolvido sem duplicar dados
do SerializedType no stream.

Fixtures sintéticas cobrem dois objetos, big/little endian, alinhamento, typeIDs,
ranges, tabela vazia e truncamento. A referência estrutural é AssetStudio
[`SerializedFile.cs`](https://github.com/Kappu12/AssetStudio/blob/main/AssetStudio/SerializedFile.cs),
avaliada CLEAN pelo Thoth (SA-0e022693d435ebdbac9f557852e2b1ca74a52e79).
