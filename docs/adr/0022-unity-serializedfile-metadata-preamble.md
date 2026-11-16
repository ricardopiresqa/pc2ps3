# ADR-0022: Leitura do preâmbulo SerializedFile

- Status: Accepted
- Data: 2026-11-02

## Contexto

O cabeçalho validado localiza metadata e indica seu endian. O próximo estágio
precisa conhecer a versão do Unity, plataforma-alvo e disponibilidade de
TypeTree para selecionar um parser compatível.

## Decisão

Para formatos 9–21, ler apenas a string Unity version terminada em NUL, limitada
a 64 bytes ASCII, o target platform como int32 assinado e a flag TypeTree como
byte 0/1. Todas as leituras ficam dentro do metadata range validado pelo header.

## Consequências

- A seleção futura de formato pode usar a versão e a flag sem carregar objetos.
- TypeTree, tipos, object table, referências PPtr e payload continuam fora do
  contrato.
- Os testes usam metadata sintética; apenas os três valores do preâmbulo local
  são consultados em memória.
