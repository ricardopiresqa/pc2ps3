# QA da Fase 4

## FidelityTrace

Feature: registrar snapshots de fidelidade em ticks deterministas

Scenario: armazenar amostras validas na ordem
Given um buffer emprestado vazio com capacidade finita
When o chamador registra dois ticks crescentes
Then o trace preserva todos os campos e a ordem de submissao

Scenario: rejeitar tick repetido ou regressivo
Given um trace com uma amostra valida
When o chamador registra um tick igual ou anterior
Then a amostra e o count existentes permanecem intactos

Scenario: rejeitar capacidade cheia
Given um buffer ja cheio
When o chamador registra outra amostra
Then a operacao falha sem escrita fora do buffer

Scenario: rejeitar armazenamento invalido
Given ponteiro nulo, capacidade zero ou acima do limite
When o trace e consultado ou recebe uma amostra
Then a operacao falha sem acesso ao armazenamento

Scenario: limpar e reutilizar o trace
Given um trace com amostras
When o chamador limpa e registra novamente a partir do tick zero
Then o count reinicia e a nova sequencia e aceita

## Fidelity comparator

Feature: comparar traces de fidelidade com tolerancias explicitas

Scenario: traces identicos passam
Given duas sequencias nao vazias com ticks iguais
When todos os campos categóricos e numericos correspondem
Then o resumo marca PASS e reporta a quantidade comparada

Scenario: erro numerico dentro do limite passa
Given divergencias de posicao, velocidade e camera nos limites configurados
When as duas sequencias sao comparadas
Then o resumo PASS inclui o maior erro observado por categoria

Scenario: erro numerico acima do limite falha
Given ao menos um eixo excede sua tolerancia
When as duas sequencias sao comparadas
Then o resumo marca divergencia sem rejeitar a estrutura do trace

Scenario: campos categoricos divergentes falham
Given input, colisao, estado, animacao, progressao ou audio diferentes
When as duas sequencias sao comparadas
Then a categoria divergente e contada e o resumo nao passa

Scenario: entrada invalida nunca passa
Given ticks incompativeis, trace vazio ou tolerancia invalida
When a comparacao e solicitada
Then retorna erro e preserva o resultado anterior

Scenario: armazenamento alterado nao quebra a ordem do trace
Given o dono do buffer altera um tick depois de registrar as amostras
When a sequencia deixa de ser estritamente crescente
Then a comparacao retorna erro e preserva o resultado anterior

## PerformanceBudgetTracker

Feature: agregar frames e memoria sem inventar contadores ausentes

Scenario: calcular FPS e picos de memoria conhecidos
Given um orcamento explicito e amostras de frame, RAM e VRAM
When o tracker recebe a amostra
Then publica FPS derivado, uso atual e picos observados

Scenario: contar violacoes sem perder o pico anterior
Given uma amostra excede RAM ou VRAM e a seguinte fica abaixo do limite
When ambas sao registradas
Then a violacao continua contada e o pico preserva o maior valor

Scenario: memoria indisponivel permanece desconhecida
Given o backend nao fornece um contador de memoria
When o tracker recebe elapsed_ns com esse contador ausente
Then current_known fica falso, sem zerar o pico nem contar violacao

Scenario: amostra de frame invalida nao atualiza memoria
Given um tracker com estado conhecido
When elapsed_ns e zero ou acima do limite
Then toda a amostra e rejeitada sem atualizar frames ou contadores

Scenario: reset limpa amostras e conserva o orcamento
Given o tracker recebeu frames e memoria
When reset e chamado
Then estatisticas e violacoes limpam e os limites continuam configurados

## FidelityTrace JSON

Feature: validar traces numericos limitados sem ecoar dados privados

Scenario: trace valido normaliza campos e tick
Given um documento JSON v1 com amostras numericas ordenadas
When a ferramenta valida o documento
Then devolve schema normalizado e resumo com count e ticks inicial/final

Scenario: rejeitar campos extras, tipos e ranges invalidos
Given campos desconhecidos, booleanos, floats ou inteiros fora do range
When a ferramenta valida o trace
Then rejeita com erro que identifica o campo sem ecoar o valor

Scenario: rejeitar chaves duplicadas e JSON invalido
Given JSON com chave repetida, constante nao padrao ou UTF-8 invalido
When a ferramenta le o arquivo
Then rejeita sem retornar resumo PASS

Scenario: limitar bytes e quantidade de amostras
Given arquivo ou array acima dos limites configurados
When a ferramenta tenta validar
Then rejeita antes de processar todo o conteudo

Scenario: CLI nao publica caminho de origem
Given arquivo de trace com caminho e nome locais
When a CLI valida e resume
Then a saida contem somente schema, count e ticks inicial/final

## CLI de comparacao

Feature: comparar dois traces offline sem confundir erro com PASS

Scenario: traces iguais retornam sucesso
Given dois arquivos v1 com campos e ticks iguais
When a CLI compara com tolerancia zero
Then imprime JSON PASS deterministico e exit code zero

Scenario: tolerancia inclusiva e divergencia numerica
Given erros numericos iguais ou maiores que a tolerancia
When a CLI compara os traces
Then igualdade no limite passa e erro acima retorna DIVERGED com exit code um

Scenario: campos categoricos exigem igualdade exata
Given input, colisao, estado, animacao, progressao ou audio diferentes
When os traces sao comparados
Then a categoria e contada como divergente mesmo com tolerancia numerica alta

Scenario: tick incompatível ou trace vazio e erro de entrada
Given sequencias de ticks diferentes ou vazias
When a CLI compara
Then retorna exit code dois e nao imprime veredito PASS

Scenario: arquivo invalido nao publica caminho local
Given baseline valido e candidato malformado
When a CLI falha ao ler o candidato
Then stdout fica vazio e stderr nao revela o caminho

## Relatorio de desempenho

Feature: relatar amostras sem inventar memoria ou PASS

Scenario: agregar FPS, intervalos de frame e picos
Given amostras completas de elapsed_ns, RAM e VRAM
When o CLI resume o arquivo
Then calcula FPS, media/min/max, picos e contagens sem ultrapassar limites

Scenario: memoria ausente resulta em NOT_PROVEN
Given ao menos uma amostra RAM ou VRAM desconhecida
When nao existe violacao conhecida
Then o CLI retorna NOT_PROVEN e nao converte ausencia em zero ou PASS

Scenario: violacao conhecida resulta em OVER_BUDGET
Given uma amostra conhecida excede RAM ou VRAM, mesmo com dados parciais
When o CLI resume o arquivo
Then retorna OVER_BUDGET e exit code um

Scenario: entrada invalida nao publica resultado
Given budget, duracao, tipo, schema ou tamanho fora dos limites
When o CLI valida o relatorio
Then stdout fica vazio e o exit code e dois

Scenario: resumo nao revela caminho e e deterministico
Given um arquivo local valido
When o CLI roda duas vezes
Then JSON e igual e nao contem caminho de origem

## Execucao local dos modulos C++ (2026-11-03)

Tifa definiu e executou os cenarios acima por meio dos testes CTest correspondentes:

- FidelityTrace: ordem e preservacao de campos, tick repetido/regressivo, capacidade cheia, armazenamento invalido e clear/reuso.
- Comparator: igualdade, tolerancias inclusivas, tolerancia excedida, igualdade exata categorica, entradas invalidas sem mutacao do resultado e armazenamento nao monotônico.
- PerformanceBudgetTracker: FPS/picos, contagem de violacoes, memoria indisponivel sem falso zero, rejeicao sem atualizacao parcial, orcamentos invalidos e reset.

Revalidação de 2026-11-03: os três alvos foram compilados com MSVC 14.44; `fidelity_trace`, `fidelity_compare`, `performance_budget`, `fidelity_trace_json` e `fidelity_trace_comparison` passaram (5/5). A regressão CTest completa passou em 70/70. C++11 estrito permanece NOT_PROVEN: MSVC não oferece modo C++11, GCC/Clang não estão no PATH, WSL retornou `E_ACCESSDENIED` e o daemon Docker Linux está indisponível. A análise sintática LLVM 19 registrada anteriormente não linka nem executa os binários.
