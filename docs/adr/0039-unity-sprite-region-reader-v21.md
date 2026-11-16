# ADR 0039 - leitor bounded de regiao Sprite para Unity v21

- Status: accepted
- Data: 2026-11-03
- TaskSpec: `pc2ps3-phase3-sprite-region-reader-v21`

## Contexto

O inspector ja identifica vinculos Sprite para Texture2D, mas nao retorna o retangulo `m_Rect` necessario para recortar pixels. Os dois Sprites encontrados nos arquivos de um build Unity usam Texture2D local direta, sem atlas associado.

## Decisao

Adicionar `read_sprite_region`, restrito a SerializedFile v21, Unity 2019.4.4f1, plataforma 19 e classID 213. A API percorre somente o prefixo bounded do objeto, valida `m_Rect` como coordenadas finitas e integrais dentro da Texture2D local e retorna apenas `x`, `y`, `width` e `height`.

Referencias externas, null/unresolved, atlas tags/ponteiros, coordenadas fracionarias, regioes vazias ou fora da textura falham sem resultado parcial. Nomes, GUIDs, IDs, paths e pixels nao fazem parte do retorno. Suporte a SpriteAtlas e layouts de outros builds fica separado ate haver fixtures e dados verificaveis.

## Validacao

Fixtures little/big-endian, limites, truncamento e rejeicoes passaram: 83 testes focados e 80 subtests. A regressao PC2PS3 passou com 262 testes, 7 skips e 203 subtests. Inspecao dos quatro arquivos locais confirmou duas regioes de Sprite validas sem emitir identificadores. Argus e Tifa aprovaram; cinco criterios Thoth estao PASS.
