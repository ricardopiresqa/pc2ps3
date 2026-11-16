# ADR 0003 — Fronteira de arquivos e formato de saves
- Status: decisão de arquitetura para implementação; validação pendente.
- Escopo: TaskSpec `pc2ps3-phase2-filesystem-save`.

## Contexto
O runtime precisa persistir slots sem depender de APIs de host ou PS3. Um parser não pode confiar no comprimento declarado no próprio arquivo, nem ler um arquivo ilimitado antes de aplicar o tamanho configurado.

## Decisões
1. `FileSystem` é uma interface independente de plataforma para leitura completa limitada, gravação atômica e remoção. `read_all(path, max_bytes, out)` deve parar e retornar `too_large` sem acumular mais que o limite; em falha, deixa `out` inalterado.
2. `SaveManager` mantém referência não proprietária ao backend e recebe um root lógico confiável, quantidade de slots e limite máximo de payload. Seu contrato exige que esses valores de configuração permaneçam válidos durante a vida do manager.
3. O chamador escolhe somente um índice numérico de slot; o manager gera o nome fixo dentro do root. Não há API para caminhos de save arbitrários fornecidos pelo jogo.
4. O envelope binário versão 1 tem magic fixo, versão, tamanho little-endian de 32 bits e CRC-32/IEEE do payload. O CRC detecta corrupção acidental; não fornece autenticidade ou proteção contra adulteração intencional.
5. O payload por slot tem teto de 16 MiB para limitar a memória ocupada pelo buffer e pelo envelope enquanto salva em uma máquina com memória restrita. O tamanho configurado também deve caber no campo de 32 bits e em `size_t` incluindo o header. A leitura é limitada ao header mais o máximo configurado. Comprimento exato, versão conhecida e CRC correto são obrigatórios; bytes extras e registros truncados são inválidos.
6. `load` decodifica em buffer temporário e só substitui a saída do chamador depois da validação integral. Payload vazio é válido.
7. O backend de `write_atomic` promete que uma falha não substitui nem corrompe o registro anterior. O manager nunca remove o registro antes de gravar.
8. Erros de I/O, ausência de slot, excesso de tamanho, configuração/slot inválido, versão não suportada e dados corrompidos têm resultados distinguíveis.

## Alternativas consideradas
- Ler o arquivo sem limite e validar depois: permite alocação proporcional a entrada malformada.
- Aceitar paths escolhidos pelo jogo: amplia desnecessariamente a superfície de traversal e acoplamento.
- Remover o arquivo anterior antes de escrever: perde o último save válido se a nova escrita falhar.
- Adicionar assinatura ou hash criptográfico: não há requisito de autenticidade para saves locais nesta etapa.

## Consequências
A atomicidade concreta fica a cargo do adaptador de plataforma e não é presumida sem implementação e teste do backend real. A interface permite backend em memória nos testes. O módulo não acessa disco diretamente e não fornece uma migração de saves comerciais. `load` remove o header validado do buffer local e o transfere para a saída sem criar uma segunda cópia do payload.

## Evidências necessárias
- Testes permanentes para limite de leitura, slots, round-trip, payload vazio, arquivo truncado/extra, magic/versão/CRC, I/O falho e preservação da saída e do save anterior.
- Revisões de segurança e código, execução de CTest e QA independente; etapa sem toolchain permanece bloqueada.

## Rollback
Remover `FileSystem`/`SaveManager`, alvo de teste e documentação desta TaskSpec. Nenhum save existente é migrado ou apagado.
