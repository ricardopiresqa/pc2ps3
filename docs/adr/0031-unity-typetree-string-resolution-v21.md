# ADR 0031: resolução de strings do Unity TypeTree

- **Status:** aceita
- **Data:** 2026-11-03
- **TaskSpec local:** `pc2ps3-phase3-unity-typetree-string-resolution-v21`
- **ClickUp:** `86e3y7c2e`

## Contexto

O parser TypeTree preserva offsets de `type` e `name`. O bit alto indica uma
referência à common-string table; offsets sem esse bit apontam para o buffer
local do TypeTree. Resolver ambos sem respeitar limites poderia ler bytes fora
do buffer ou aceitar strings truncadas.

## Decisão

Adicionar `resolve_type_tree_string_offset` para resolver um offset local ou
common. O chamador fornece os buffers explicitamente; a ferramenta não embute
nem busca uma tabela de terceiros. Cada string precisa terminar em NUL, ter até
4096 bytes e decodificar como UTF-8 estrito. O bit alto é removido antes de
consultar o common-string buffer. `resolve_type_tree_node_strings` resolve
separadamente os campos `type` e `name` sem mutar o node.

## Consequências

- Os offsets comuns continuam inertes até o chamador fornecer a tabela correta.
- Buffers acima do limite, offsets ausentes, terminadores ausentes e UTF-8
  inválido falham com `UnitySerializedFileError`.
- Nenhuma dependência ou cópia de conteúdo de terceiros é necessária.

## Evidência

- Fixtures sintéticas cobrem offsets locais/common, type/name, string vazia,
  limites, truncamento, índice ausente e UTF-8 inválido.
- Resultado do teste focado e suíte completa é registrado no comentário QA do
  ClickUp.
