# ADR 0038 — Decoder BC7 bounded para Texture2D v21

- **Status:** accepted
- **Data:** 2026-11-03
- **TaskSpec:** `pc2ps3-phase3-bc7-decoder-v22`
- **ClickUp:** aguardando limite da API; card local em `docs/LOCAL_CARD_pc2ps3-phase3-bc7-decoder-v22.md`.

## Contexto

O inspector validou um payload BC7 carregado do build Unity analisado com 129.600 blocos nos modos 0, 1, 3, 4, 5 e 6. O conversor RGBA8 suportava somente Alpha8, RGB24 e RGBA32. O processamento precisa permanecer em memória, com os mesmos limites da API Texture2D, e não deve produzir pixels na CLI.

## Decisao

Estender `decode_texture2d_to_rgba` para TextureFormat 25 nos seis modos observados. O decoder interpreta blocos de 128 bits LSB-first, as particoes e texels-ancora, endpoints, P-bits, indices, pesos oficiais de interpolacao e rotacao de canais dos modos 4 e 5. O mip base e recortado para as dimensoes reais; os bytes dos mips restantes sao validados e ignorados.

Os modos 2 e 7 e prefixos sem bit unary valido sao rejeitados explicitamente. Truncamento, tamanho inconsistente e limites excedidos falham sem retornar buffer parcial. A API externa carregada reutiliza o mesmo conversor; a saida JSON do inspector permanece agregada e sem pixels.

As tabelas de particao/fix-up foram adaptadas de `bcdec.h` v0.985, publicado sob Unlicense. A logica segue a documentacao oficial [BC7 Format](https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc7-format). Avaliacoes de seguranca Thoth: `SA-76da3a6816d82281f56bf7be01b586a61a1600da` (Microsoft), `SA-ba92aed8d3cbf9fcbb9f307d34820dd4eb6adaf2` (bcdec) e `SA-cd6629aa300e1fc735b11f88237bf5d750f37ae4` (Khronos BPTC).

## Consequencias e limites

- Texturas Unity com os seis modos medidos podem ser convertidas em RGBA8 em memoria.
- BC7 generico continua parcial enquanto os modos 2 e 7 nao forem implementados.
- Exportacao de sprites, atlas, runtime PS3, PKG e validacao RPCS3 continuam fora deste card.

## Evidencia

- `python -m unittest discover -s tests -p test_unity_texture_pixels.py -v` — 17 testes passaram.
- A suite completa, QA Tifa e revisao Argus serao registrados apos a validacao final deste card.

## Validacao final

O decoder foi confirmado com o payload BC7 local do build Unity analisado: 129.600 blocos agregados, distribuicao nos modos 0, 1, 3, 4, 5 e 6 e zero prefixos invalidos. Uma textura 1920x1080 foi convertida para 8.294.400 bytes RGBA em memoria. Testes focados: 27; regressao PC2PS3: 258 passed, 7 skipped e 190 subtests. Argus e Tifa aprovaram; os cinco criterios da TaskSpec estao PASS. A saida nao persistiu nem exibiu pixels.
