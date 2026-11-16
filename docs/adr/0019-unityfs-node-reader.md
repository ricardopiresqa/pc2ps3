# ADR 0019. Leitura limitada de nodes UnityFS

Data: 2026-11-03 · Status: aceita para implementação local

## Contexto

O inventário UnityFS precisa continuar lendo somente header e block-info. A
vertical slice Unity precisa, em seguida, acessar nodes individuais para
conversão local; carregar ou descomprimir o bundle inteiro seria caro e
ampliaria o impacto de uma entrada malformada.

## Decisão

- Manter `tools/unityfs_header.py` como parser de metadados e offsets, sem ler
  payload.
- Ler um node por vez em `tools/unityfs_data.py`, usando os offsets validados e
  descomprimindo apenas blocos que intersectam o node.
- Suportar somente blocos sem compressão, LZ4 e LZ4HC nesta etapa; rejeitar
  LZMA, criptografia e métodos desconhecidos.
- Limitar cada bloco de saída a 128 MiB e cada node a 256 MiB. A função retorna
  bytes somente após completar e validar todo o node; não grava arquivos.

## Alternativas consideradas

- Fazer o parser de header também ler payload: rejeitada para preservar o
  inventário metadata-only.
- Descomprimir bundles inteiros: rejeitada pelo uso de memória e pela leitura
  desnecessária de nodes não selecionados.
- Adicionar UnityPy ou outra dependência: rejeitada; o LZ4 local já cobre os
  blocos suportados e mantém o pipeline controlado.

## Consequências

- Nodes simples podem alimentar conversores posteriores sem carregar o bundle
  inteiro.
- Nodes maiores que o limite, LZMA e bundles criptografados permanecem
  explicitamente sem suporte nesta etapa.
- A gravação em disco e a validação de caminhos ficam para um CLI separado.
