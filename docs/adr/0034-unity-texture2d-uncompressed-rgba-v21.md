# ADR 0034 — Conversão bounded de Texture2D inline para RGBA8

## Contexto

O leitor v21 já valida e retorna payloads Texture2D inline em memória. O inventário local do build Unity analisado encontrou Alpha8 inline e também formatos RGB24, RGBA32 e BC7 em estados externos ou vazios. O runtime de renderização consome pixels RGBA8.

## Decisão

Adicionar `decode_texture2d_to_rgba` para converter o nível base de Alpha8 (1), RGB24 (3) e RGBA32 (4). Alpha8 vira branco com alpha vindo dos dados; RGB24 recebe alpha opaco; RGBA32 preserva os bytes. A função valida o tamanho completo declarado, incluindo a cadeia de mipmaps, antes de retornar o nível base.

Limitar dimensões a 4096, pixels a 8.388.608 e saída a 32 MiB. Rejeitar storage externo/vazio, inconsistências e formatos não suportados. Não escrever arquivos nem imprimir bytes de pixel. BC7 e exportação de sprites ficam em tarefas próprias.

## Consequências

- A conversão é pura e mantém a ordem de pixels do payload.
- O processamento fica limitado antes da alocação e falha sem retorno parcial.
- Texturas BC7 ainda não podem alimentar o renderer por esta API.

## Verificação

Fixtures sintéticas cobrem os três formatos, mip tail validado, storage inválido, truncamento, dimensões, BC7 e limites configuráveis. A suíte completa verifica o pipeline Python.
