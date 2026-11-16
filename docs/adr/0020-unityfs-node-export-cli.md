# ADR-0020: Exportação de nodes UnityFS por índice

- Status: Accepted
- Data: 2026-11-02

## Contexto

O leitor limitado já devolve bytes de um node, mas ferramentas posteriores
precisam gravar alguns nodes localmente. Paths e nomes no diretório UnityFS são
dados externos e não podem escolher destinos no filesystem.

## Decisão

Adicionar uma CLI que recebe o bundle, o diretório de saída e índices numéricos.
Os nomes são gerados como `node_000001.bin`; a CLI usa o layout validado e
`read_node_bytes`, verifica os limites antes de ler e recusa links/reparse points
e arquivos de destino existentes. Cada arquivo é escrito em um temporário no
mesmo diretório e publicado com criação atômica sem sobrescrita. Uma falha na
publicação remove os arquivos desta seleção que já haviam sido criados.

## Consequências

- A seleção é explícita e não depende de nomes internos do bundle.
- Filesystems sem suporte a hard links falham sem publicar payload parcial.
- A CLI não interpreta nem executa o conteúdo exportado.
