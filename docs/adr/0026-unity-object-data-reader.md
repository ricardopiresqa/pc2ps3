# ADR-0026: Leitura bounded de payloads de objetos Unity v21

- Status: Accepted
- Data: 2026-11-03

## Contexto

A object table v21 já valida `byteStart` e `byteSize` contra o `file_size`, mas
o pipeline ainda precisa ler os bytes de um objeto escolhido sem avançar por
payloads vizinhos, decodificar dados ou exportar arquivos.

## Decisão

Adicionar `read_serialized_object_payload`, que aceita somente um header v21
estruturalmente válido e um `UnitySerializedObject` com campos dentro dos limites
representáveis pelo formato. A função resolve o início absoluto como
`data_offset + byte_start`, valida a faixa novamente contra `file_size` e
`MAX_SERIALIZED_FILE_BYTES`, posiciona o stream e lê exatamente `byte_size`
bytes usando `_read_exact`.

Um payload de tamanho zero retorna `b""`. Stream truncado, header inválido,
registro inválido ou faixa excedente lançam `UnitySerializedFileError`; nenhum
resultado parcial é retornado e nenhuma gravação em disco é feita.

## Consequências

O chamador recebe bytes brutos em memória para uma única entrada da object table.
Decodificação de objetos e exportação continuam fora do escopo deste leitor.
`CountingStream` cobre offsets, quantidade de bytes lida, payload vazio, limites
no início/fim do arquivo, objeto vizinho, range inválido e truncamento.
