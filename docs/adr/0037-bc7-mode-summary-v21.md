# ADR 0037 — Histograma agregado de modos BC7

## Contexto

O Unity contém um payload Texture2D BC7 external de 2.073.600 bytes. Antes de implementar a conversão de blocos, é necessário conhecer os modos presentes sem exportar nem expor pixels.

## Decisão

Adicionar `--bc7-mode-summary`, exigindo o resumo de payload e uma raiz externa explícita. Para cada bloco validado de 16 bytes, classificar o prefixo unary LSB no primeiro byte em modo 0 a 7; se o byte inicial for zero, contar prefixo inválido. Emitir somente número de texturas/blocos e histogramas agregados.

O resumo reaproveita as validações de SerializedFile, path e budget do inspector e não decodifica nem salva imagem.

## Consequências

- O histograma permite priorizar os modos usados pelo Unity para a próxima implementação.
- Prefixos inválidos e payload desalinhado falham/contam separadamente sem expor o bloco.
- Os resultados não são uma conversão de imagem e não substituem QA visual.

## Verificação

Fixtures sintéticas cobrem os oito prefixos, um prefixo inválido, payload desalinhado e requisitos das flags. Nos dados de um build Unity, 129.600 blocos BC7 usam modos 0, 1, 3, 4, 5 e 6; não há prefixos inválidos.
