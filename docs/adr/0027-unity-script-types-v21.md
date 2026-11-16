# ADR-0027: Leitura bounded da script types table Unity v21

- Status: Accepted
- Data: 2026-11-03

## Contexto

O parser da object table v21 retorna o cursor antes da tabela de scripts. Essa
tabela associa um índice de arquivo a um identificador local e deve ser lida
antes das referências externas, sem consumir as tabelas seguintes.

## Decisão

Implementar `parse_serialized_script_type_table` somente para header v21 com
TypeTree habilitada e object table validada no cursor atual. O count int32 é
limitado por `MAX_SERIALIZED_OBJECTS`. Cada entrada preserva `fileIndex` int32,
alinha o cursor a 4 bytes e lê `localIdentifierInFile` como int64 no endian do
header. Cada leitura permanece dentro de `metadata_end`.

Truncamento, count inválido ou cursor inconsistente lançam
`UnitySerializedFileError`; o parser não retorna tabelas parciais nem resolve
`fileIndex` para caminhos externos.

## Consequências

O resultado contém registros imutáveis e o cursor exato antes da tabela de
externals. Fixtures cobrem endian little/big, IDs negativos/signed, alinhamento,
tabela vazia, cursor, count e truncamento. Referência estrutural: [AssetStudio
SerializedFile.cs](https://github.com/Kappu12/AssetStudio/blob/main/AssetStudio/SerializedFile.cs).
