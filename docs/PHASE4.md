# Fase 4 — fidelidade e desempenho

## Traces por tick

`FidelityTrace` registra amostras fornecidas pelo chamador em armazenamento emprestado. Cada amostra pode representar input, posicao/camera em milésimos de unidade, velocidade em milésimos de unidade por segundo, colisao, estado e animacao, checkpoint/fase e eventos de audio. IDs e bitmasks sao opacos e definidos pelo jogo ou backend; eixos mantêm o valor inteiro fornecido pelo capturador.

O dono do buffer precisa mantê-lo vivo enquanto o trace for usado. O core nao aloca nem consulta APIs de plataforma. A captura fornece dados para comparar baselines futuros; ainda nao representa uma medicao de um build Unity, de backend PS3 ou de hardware.

## Comparacao

`compare_fidelity_traces` compara sequencias nao vazias com ticks iguais e ordenados. Posição, velocidade e camera usam tolerancias inteiras; input, colisao, estado, animacao, progressao e eventos de audio exigem igualdade exata. Um resultado invalido retorna erro e nao pode ser interpretado como PASS. Um trace sintetico e uma ferramenta de verificacao, nao uma captura real de um build Unity/PS3.

## Orcamento de desempenho

`PerformanceBudgetTracker` reutiliza a janela de `FrameTimeTracker` e recebe contadores de RAM/VRAM do chamador. Um contador pode estar indisponivel: nesse caso o valor atual e desconhecido, nao zero, e nao conta como violacao. O chamador deve consultar `*_current_known()` antes de usar `*_current_bytes()`; o core nao consulta APIs de plataforma. FPS, picos e orcamentos so refletem amostras realmente fornecidas.

## Arquivos de trace

`tools/fidelity_trace_json.py` valida schema JSON v1 limitado a 16 MiB e 65.536 amostras. O formato contem somente campos numericos de `FidelitySample`, exige ticks crescentes e rejeita campos extras, duplicados ou valores fora da faixa C++ correspondente. A ferramenta nao grava nem publica paths de origem.

## Comparacao offline

`tools/compare_fidelity_trace.py` compara dois arquivos v1 e aplica a mesma regra de tolerancias/igualdade do core. Exit 0 significa PASS, exit 1 divergencia valida e exit 2 entrada ou sequencia invalida. A saida e JSON deterministico sem nomes de arquivo nem valores completos do trace.

## Relatorio offline de desempenho

`tools/performance_report.py` agrega o arquivo inteiro: media/min/max de frame, FPS inteiro, picos e violacoes de RAM/VRAM. Memoria desconhecida usa null e impede PASS quando falta qualquer amostra; uma violacao conhecida retorna OVER_BUDGET mesmo em relatorio parcial. Exit codes: 0 PASS, 1 OVER_BUDGET, 2 entrada invalida, 3 NOT_PROVEN. O formato e limitado a 16 MiB e 65.536 amostras e nao afirma que contadores vieram de PS3.

## Validacao CMake/CTest do core C++ (2026-11-03)
O CMake configurou o projeto com o gerador Visual Studio 17 2022 e MSVC 19.44.35228.0. O core e os três alvos Fase 4 foram compilados; `fidelity_trace`, `fidelity_compare` e `performance_budget` passaram no CTest (3/3). Na revalidação de 2026-11-03, os três alvos e os testes Python de trace passaram (5/5), e a regressão host completa passou em 70/70. O build do core ainda emite um aviso C4244 em `src/core/audio.cpp`, fora destes três módulos.

O projeto solicita C++11 em CMake, mas o projeto MSVC gerado nao declara `LanguageStandard`. A análise sintática LLVM 19 `clang-tidy` registrada anteriormente não linka nem executa binários. Nesta sessão, MSVC 14.44 compilou e executou CTest no host; GCC/Clang não estavam no PATH, WSL retornou `E_ACCESSDENIED` e o daemon Docker Linux não estava disponível. Portanto, build e execução sob C++11 estrito continuam NOT_PROVEN. Nenhuma amostra real de um build Unity, backend PS3, RAM ou VRAM foi medida. A regressão geral CTest passou em 70/70; detalhes em `QA_HOST_FULL.md`.
