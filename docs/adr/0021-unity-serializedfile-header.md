# ADR-0021: Leitura limitada do cabeçalho Unity SerializedFile

- Status: Accepted
- Data: 2026-11-02

## Contexto

Nodes UnityFS podem conter SerializedFiles. A etapa de exportação só lê bytes;
um leitor de metadata precisa conhecer os limites do arquivo e sua endianess
antes de interpretar conteúdo variável.

## Decisão

Implementar somente o cabeçalho fixo de 20 bytes para formatos Unity 9–21,
observado o formato 21 em um bundle da instalação Unity 2019.4.4f1. Os campos
fixos são lidos em big-endian; o marcador do cabeçalho determina a endianess dos
metadados. Tamanhos e offsets são validados contra o tamanho disponível e o
limite de 256 MiB. A implementação não lê metadata nem payload.

## Consequências

- A etapa seguinte pode consumir um envelope validado sem reler o bundle.
- Versões 22+, TypeTree, tabela de objetos e PPtr permanecem fora do contrato.
- Os testes usam cabeçalhos sintéticos e não armazenam nodes comerciais.
