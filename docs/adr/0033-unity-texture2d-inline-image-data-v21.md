# ADR 0033 — Leitura bounded de dados Texture2D v21

## Contexto

Os quatro SerializedFiles locais do build Unity usam Unity 2019.4.4f1, plataforma 19 e v21 sem TypeTree. A leitura existente validava dimensões, tamanho completo e TextureFormat, mas não identificava se os bytes estavam no próprio objeto ou em um stream externo.

## Decisão

Adicionar `read_texture2d_image_data` para esse layout específico. A função valida o objeto e a faixa, calcula o tamanho esperado por dimensão, mip count e TextureFormat, e aceita apenas os formatos observados: Alpha8 (1), RGB24 (3), RGBA32 (4) e BC7 (25). O limite por imagem é 64 MiB.

Dados inline são lidos exatamente para memória. Dados externos retornam status e tamanho, sem retornar ou resolver o caminho e o offset do stream. Objetos vazios retornam status próprio. A CLI emite somente contagens agregadas por formato e armazenamento; não exporta pixels.

## Consequências

- Leitura truncada, overflow, contagem incompatível ou layout diferente falha sem resultado parcial.
- O parser não abre arquivos apontados por `m_StreamData`.
- A conversão de pixels, o decoder BC7 e a exportação de sprites continuam em tarefas posteriores.

## Verificação

Fixtures little/big endian cobrem formatos inline, stream externo, objeto vazio, mip chain, truncamento, limites e caminho externo não emitido. O inspector foi executado sobre os quatro arquivos locais e produziu somente o agregado registrado em `docs/PHASE3.md`.
