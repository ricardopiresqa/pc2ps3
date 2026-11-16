# ADR 0030: parser agregado do metadata Unity SerializedFile v21

- **Status:** aceita
- **Data:** 2026-11-03
- **TaskSpec local:** `pc2ps3-phase3-unity-metadata-reader-v21`
- **ClickUp:** `86e3y7c2a`

## Contexto

Os parsers bounded v21 já leem, em etapas isoladas, o header, preâmbulo,
SerializedType, object table, script types, externals, refTypes e
`userInformation`. Os consumidores ainda precisavam coordenar cursores e
passar as tabelas intermediárias manualmente.

## Decisão

Adicionar `parse_serialized_file_metadata_v21(stream, available_size)` como
orquestrador das etapas existentes. A função aceita apenas v21 com TypeTree
habilitada, inicia no offset zero e retorna todas as tabelas em um resultado
único somente após validar que o cursor final coincide com `metadata_end`.

O orquestrador não lê payloads de objetos, não resolve referências externas e
não devolve resultado parcial. Falhas das etapas internas continuam como
`UnitySerializedFileError`.

## Consequências

- Consumidores passam a ter um caminho único e determinístico para a metadata
  v21 completa.
- O limite de leitura continua sendo o range declarado de metadata; bytes de
  payload permanecem intocados.
- Formatos sem TypeTree ou diferentes de v21 continuam fora deste agregador.

## Evidência

- Fixtures little/big endian cobrem tabelas vazias e não vazias, cursor final,
  truncamento e ausência de leitura do payload.
- Testes focados e suíte completa são registrados no comentário QA do ClickUp.
