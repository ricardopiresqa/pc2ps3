# ADR-0023: Leitura limitada do blob TypeTree

- Status: Accepted
- Data: 2026-11-02

## Contexto

O SerializedFile v21 local declara TypeTree habilitada. Sua representação blob
tem uma contagem de nós, uma tabela plana de records e um string buffer. O parser
precisa validar esse envelope sem seguir offsets inválidos nem interpretar o
payload dos objetos.

## Decisão

Para formatos 19–21, parsear records de 32 bytes com versão, level, flags,
offsets de tipo/nome, byte size, index, meta flags e ref type hash. Limitar a
100.000 nodes e 1 MiB de strings; validar a faixa antes de ler a tabela. Exigir
root level 0, profundidade até 64 e saltos de no máximo um nível. Validar offsets
locais no string buffer e preservar referências common-string sem resolvê-las.

A estrutura foi conferida com o parser público [AssetStudio `SerializedFile.cs`](https://github.com/Kappu12/AssetStudio/blob/main/AssetStudio/SerializedFile.cs);
nenhum código ou asset foi copiado.

## Consequências

- O primeiro blob do node v21 local pode ser indexado: 58 nodes e 242 bytes de
  string buffer.
- A resolução de common strings, a tabela completa de tipos e a object table
  são etapas futuras.
- Nodes sintéticos são usados nos testes permanentes.
