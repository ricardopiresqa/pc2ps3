# ADR 0029: Unity reference types e metadata final v21

- **Status:** aceita
- **Data:** 2026-11-03
- **TaskSpec:** `pc2ps3-phase3-unity-reference-types-v21`

## Contexto

O parser v21 já percorre cabeçalho, preâmbulo, tipos, object table, script
types e externals com limites de metadata. O cursor após externals precisa
consumir `refTypes` e `userInformation` sem invadir payloads ou a região de
dados.

## Decisão

Implementar `parse_serialized_reference_type_table` como etapa bounded para
SerializedFile v21 com TypeTree habilitada. Cada refType preserva `classID`,
`stripped`, `scriptTypeIndex`, `scriptID` condicional, `oldTypeHash`, TypeTree,
class name, namespace e assembly name. A string final `userInformation` é
limitada a 1 MiB. Todas as strings precisam terminar em NUL antes de
`metadata_end`.

Aplicar os limites existentes de refTypes e os limites agregados de nodes e
buffers TypeTree. Em erro, lançar `UnitySerializedFileError` sem retornar
registros parciais. O parser retorna o cursor após `userInformation`; os nomes
e caminhos permanecem dados inertes, sem abrir arquivos ou resolver referências.

## Consequências

- O layout da metadata v21 é consumido até o limite declarado sem ler dados de
  objetos.
- Endian little/big e condições de `scriptID` ficam cobertos por fixtures
  sintéticas.
- Versões diferentes de v21 e resolução de referências continuam fora deste
  parser.

## Evidência

- `python -m unittest discover -s tests -p test_unity_serialized_file.py` — 47
  testes passaram.
- `python -m unittest discover -s tests` — 177 testes passaram; 7 foram
  ignorados pela suíte.
