# ADR 0014: Transformações e lotes no Renderer2D portátil

- Status: aceito
- Data: 2026-11-02
- Decisão Thoth: `738bae3774a92832c053ab6517fd79e94892e1acf9eed4f89c6c6165ae892fb6`
- Planejamento Gaius: `8eed52c30371a73b06c4f84d50f7e2af7383de86d30e8a49078331159790f3ac`

## Contexto

O prompt do PC2PS3 inclui escala, rotação e renderização em lotes. O
`Framebuffer` portátil desenha regiões de textura em tamanho 1:1. O ambiente
não tem backend RSX/SDL2 validado, então os novos contratos precisam ser
verificáveis no rasterizador host sem afirmar aceleração de dispositivo.

## Decisão

- Adicionar desenho escalado e rotacionado por nearest-neighbor ao `Framebuffer`.
- Manter transforms como operações visuais: não alteram física ou simulação.
- Adicionar `RenderBatch` separado, limitado e pré-alocado. Ele mantém a ordem
  dos comandos e os descarrega no `Framebuffer` existente.
- `RenderBatch` guarda ponteiros emprestados; o chamador mantém cada `Texture`
  viva até `flush()` ou `clear()`.
- Validar todos os comandos antes de escrever no framebuffer. Um flush inválido
  não pode deixar desenho parcial.
- Limitar esta entrega ao backend de software. O batch não promete reduzir
  draw calls nem acelerar RSX; isso depende de um backend de dispositivo futuro.

## Consequências

- Pixels, clipping, alpha, capacidades, ordem e erros podem ser testados em
  CTest host.
- Rotação arbitrária usa cálculo visual de ponto flutuante e nearest-neighbor;
  diferenças mínimas entre bibliotecas matemáticas podem afetar bordas.
- O contrato de lifetime emprestado precisa permanecer explícito na API e nos
  testes.

## Rollback

Remover os métodos de transform do `Framebuffer`, a classe `RenderBatch`, os
alvos/testes CMake e as referências desta ADR em `PHASE2.md` e `QA_PHASE2.md`.
Não há migração de dados.
