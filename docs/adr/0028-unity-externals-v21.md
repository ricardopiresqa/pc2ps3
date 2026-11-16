# ADR-0028: Leitura bounded da tabela externals Unity v21

- Status: Accepted
- Data: 2026-11-03

## Contexto

Depois da script types, o SerializedFile v21 armazena referências a outros
arquivos. O leitor precisa preservar esses identificadores para etapas futuras,
sem abrir caminhos fornecidos pelo conteúdo do asset.

## Decisão

Adicionar `parse_serialized_external_references`, que aceita o cursor retornado
pela script table validada. Limitar o count a `MAX_SERIALIZED_EXTERNALS`; cada
registro lê `assetPath` UTF-8 com NUL, GUID bruto de 16 bytes, `type` int32 e
`pathName` UTF-8 com NUL. As duas strings são limitadas a 4096 bytes e todas as
leituras passam por `metadata_end`.

String inválida, sem NUL, registro truncado, count excessivo ou cursor
inconsistente lança `UnitySerializedFileError`; o parser não retorna tabela
parcial e não abre nem grava arquivos apontados por `pathName`.

## Consequências

O resultado é uma tupla de identificadores externos e o cursor exato antes de
`refTypes`. Fixtures cobrem GUID, endian, UTF-8, strings vazias, limites,
truncamento e sentinela posterior. Referência estrutural: [AssetStudio
SerializedFile.cs](https://github.com/Kappu12/AssetStudio/blob/main/AssetStudio/SerializedFile.cs).
