# Caligo — Arquitetura

Versão 0.1

## 1. Princípio

A arquitetura existe para que o código continue pequeno, previsível e substituível. Três regras governam tudo:

1. Um módulo tem uma responsabilidade.
2. Dependência aponta só para baixo.
3. Nenhum arquivo cresce sem limite.

## 2. Módulos

| Módulo | Responsabilidade | Depende de |
|---|---|---|
| `core` | memória, containers, log, math, jobs, tempo, resultado | — |
| `platform` | janela, input, arquivo, threads, DPI | core |
| `render` | RHI, render graph, passes, UI renderer, pós | core, platform |
| `assets` | importação, cook, GUID, manifesto, cache | core, platform |
| `scene` | entidades, componentes, serialização, undo, prefabs | core, assets |
| `physics` | Jolt, queries, character, veículos | core, scene |
| `audio` | mixer, 3D, zonas, streaming | core, assets |
| `animation` | clips, blend, IK, retarget | core, scene |
| `script` | host .NET, bindings, hot reload | core, scene, physics, audio |
| `game` | mundo, streaming, trânsito, multidão, câmera, UI | todos acima |
| `editor` | UI de ferramenta, viewport, painéis, profiler | todos acima |

Regras de fronteira:

- Só `include/caligo/<módulo>/` é público. Nenhum módulo inclui `src/` de outro.
- Header inclui o mínimo; o resto é declaração antecipada.
- Nenhum módulo importa o editor. O editor é folha.
- O script nunca inclui header de render, física ou memória. Ele fala com uma ABI C curada.

## 3. Threads

- **Main:** editor, cena, input, tick de jogo.
- **Render:** submissão e sincronização com a GPU.
- **Jobs:** culling, animação, passos de física, decode de asset.
- **IO:** carregamento assíncrono de assets.

Regras:

- Nenhum lock em caminho quente.
- Dados entre threads são double-buffered ou copiados na fronteira de fase.
- Estado mutável compartilhado só existe com contrato escrito no header.
- Simulação em tick fixo de 60 Hz; render interpolado.

## 4. Memória

- Arena linear por frame e por fase.
- Pool para componentes e recursos de render.
- `unique_ptr` para posse; ponteiro cru só para observação.
- `new` e `delete` proibidos no código de jogo.
- Alocador com rastreio em debug, relatório de vazamento no shutdown.
- Orçamento de memória por subsistema, verificado no CI.

## 5. Erros

- `Result<T>` para falha recuperável. Exceção não atravessa módulo.
- `CALIGO_ASSERT` para erro de programação em debug.
- Falha fatal: log, flush, dump e saída limpa.
- Nenhuma falha silenciosa. Toda falha tem log e teste.

## 6. Organização de arquivos

- Limite rígido: **600 linhas por arquivo**, verificado no CI. Alvo macio: 400.
- Um tipo principal por arquivo; o nome do arquivo é o nome do tipo.
- Divisão por responsabilidade: `GBufferPass.cpp`, `ShadowPass.cpp`, `TaaPass.cpp`. Nunca um `DeferredRenderer.cpp` de sete mil linhas.
- Funções com no máximo 80 linhas e no máximo 3 níveis de aninhamento.
- Header com `#pragma once`, declaração antecipada e nenhuma implementação fora de template ou inline.
- `TODO`, `FIXME`, `stub` e `placeholder` quebram o CI.
- Sem comentários. Nome e estrutura explicam.
- Formatação por clang-format, verificado no CI.
- Nomes: PascalCase para tipos e funções, camelCase para locais, `m_` para membros, `k` para constantes. Sem prefixo `I` em interface.

## 7. Testes

- doctest, um arquivo de teste por módulo.
- Unitário: matemática, containers, serialização, layout.
- Integração: cena → física → snapshot de render.
- Imagem dourada: WARP, determinística, 300 frames para sistemas temporais.
- Determinismo: tick fixo, RNG com semente, relógio de parede proibido na simulação.
- Desempenho: contadores por sistema; regressão acima de 10% quebra o CI.
- CI: Windows, Debug e Release, todos os testes, artefatos publicados.

## 8. Documentação

- `docs/ESCOPO.md` — produto, escopo, releases.
- `docs/ARQUITETURA.md` — contratos entre módulos e regras transversais.
- `docs/sistemas/*.md` — um documento por sistema, escrito na release que o implementa.
- `docs/DECISOES.md` — registro de decisões.
- `docs/rfcs/` — mudanças em decisão fixa.

A documentação segue a mesma regra do código: um arquivo, uma responsabilidade. Um escopo de cinco mil linhas é o mesmo defeito que um renderer de cinco mil linhas.

## 9. Portões de CI

- Build `/W4 /WX` em Debug e Release.
- Testes unitários e de integração.
- Imagens douradas.
- Limite de linhas por arquivo.
- Proibição de TODO, FIXME, stub e placeholder.
- clang-format.
- Checagem de fronteira de módulo.
