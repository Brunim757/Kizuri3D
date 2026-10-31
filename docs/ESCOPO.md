# Caligo — Escopo e Fundação

> Engine de mundo aberto estilizado. Windows. Cidade densa e natureza estilizada. Nada além.

Versão 0.3

## 1. Identidade

Caligo existe para produzir dois tipos de mundo:

- **Cidade densa** — quarteirões, ruas, veículos, trânsito, multidão, interiores.
- **Natureza estilizada** — terreno aberto, vegetação, escalada, planeio, luz de céu aberto.

A referência de qualidade é a RAGE para cidade densa. A ambição de identidade é ser a melhor engine de mundo estilizado — o campo onde uma equipe pequena pode vencer.

"Comparável à RAGE" não é adjetivo. É medido por:

- 1 km de visão sem hitch.
- 200 NPCs e 50 veículos num quarteirão a 60 fps.
- Streaming contínuo, sem tela de carregamento.
- Iteração criar → jogar em menos de um minuto.

## 2. Decisões fixas

Estas decisões não mudam sem RFC. A regra está na seção 11.

| ID | Decisão | Status |
|---|---|---|
| D-001 | Produto primeiro: nenhum sistema entra se não aparecer numa cena jogável | FIXO |
| D-002 | Duas colunas: cidade densa e natureza estilizada | FIXO |
| D-003 | Núcleo em C++20, gameplay em C# (.NET 10 LTS) | FIXO |
| D-004 | Windows 10/11 x64 e Direct3D 11 na v1 | FIXO |
| D-005 | Um executável com editor e runtime, cena como fonte única de verdade | FIXO |
| D-006 | Sem placeholder, stub ou código morto | FIXO |
| D-007 | TDD; compilação e teste apenas no CI | FIXO |
| D-008 | Todo sistema visual tem teste de imagem determinístico | FIXO |
| D-009 | Sem comentários no código | FIXO |
| D-010 | Decisão fixa só muda por RFC escrito | FIXO |
| D-011 | Nome: Caligo, sem sufixo | FIXO |
| D-012 | Sem rede, VR, mobile, console ou macOS na v1 | FIXO |
| D-013 | Sem visual scripting | FIXO |
| D-014 | Sem engine genérica, marketplace ou plugins de terceiros | FIXO |
| D-015 | Sem ray tracing obrigatório e sem fotorrealismo | FIXO |
| D-016 | Stack de bibliotecas da seção 5, sem vcpkg | FIXO |
| D-017 | Asset fonte em formato aberto, cozido em binário próprio | FIXO |
| D-018 | Meta de 1080p60 numa GTX 1060 6 GB | FIXO |
| D-019 | Piso de hardware: FL11_0, 2 GB de VRAM, 8 GB de RAM | PROVISÓRIO |
| D-020 | UI de jogo separada do editor: markup declarativo, estilo em folha e binding por view-model | FIXO |
| D-021 | Limite rígido de linhas por arquivo, verificado no CI | FIXO |
| D-022 | Valor do limite: 600 linhas em `.cpp`, 400 em `.h`, revisado após a R2 | PROVISÓRIO |

## 3. Não-objetivos

Caligo não é e não tenta ser:

- Engine genérica ou plataforma para terceiros.
- Marketplace, loja de assets ou ecossistema de plugins.
- 2D, mobile, VR, console ou macOS.
- Rede, MMO ou co-op na v1.
- Visual scripting.
- Fotorrealismo ou ray tracing obrigatório.
- Compatível com o protótipo Kizuri. Nenhum código dele é copiado.

## 4. Plataforma e hardware

**Sistema:** Windows 10 21H2+ e Windows 11, x64.

**API gráfica:** Direct3D 11, feature level 11_0. A RHI é desenhada para receber um backend DX12 depois, mas a v1 não tem DX12.

**Compilador:** MSVC 2022 (v143), C++20, `/W4 /WX`.

**Build:** CMake + Ninja. Dependências por FetchContent com tag fixa. Sem vcpkg.

**Script:** .NET 10 LTS. Runtime self-contained no build de distribuição.

**Hardware:**

- Piso: FL11_0, 2 GB de VRAM, 8 GB de RAM.
- Alvo: GTX 1060 6 GB, 1080p60.
- Recomendado: RTX 3060 ou RX 6600.

## 5. Stack

| Área | Escolha |
|---|---|
| Janela e input | Win32 + Raw Input |
| Matemática | DirectXMath |
| Render | RHI própria sobre D3D11, HLSL |
| Física | Jolt |
| Jobs | enkiTS |
| Áudio | miniaudio + mixer 3D próprio |
| Editor | Dear ImGui docking + ImGuizmo |
| glTF | cgltf |
| Imagens | stb + DirectXTex (BCn) |
| Compressão | zstd |
| Assets cozidos | FlatBuffers |
| Malha | meshopt, mikktspace, xatlas |
| Script | .NET 10, Roslyn |
| Testes | doctest |

Cortados do protótipo: libwebp, Draco, BasisU. Não servem ao escopo e só aumentam o build.

## 6. Arquitetura

Módulos, com dependência apenas para baixo. O grafo completo, os contratos e as regras de fronteira estão em `docs/ARQUITETURA.md`.

- **core:** memória, containers, log, math, jobs, tempo.
- **platform:** janela, input, arquivo, threads.
- **render:** RHI, render graph, deferred clustered, sombras, TAA, pós.
- **assets:** importação, cook, GUID, manifesto, cache.
- **scene:** entidades, componentes, serialização, undo.
- **physics:** Jolt, queries, character, veículos.
- **audio:** mixer, 3D, zonas.
- **animation:** clips, blend, IK, retarget.
- **script:** host .NET, bindings, hot reload.
- **game:** mundo, streaming, trânsito, multidão, câmera.
- **editor:** UI, viewport, painéis, profiler.

**Fluxo de cena:** o editor edita a cena. Play congela um snapshot, roda o mundo e descarta ao parar. Nada do runtime contamina a autoria.

**Assets:** fonte em `assets/`, cozido em `cooked/`. GUID por asset, hash da fonte, cook incremental. O runtime só lê cozido.

**Script:** assembly `Caligo.Gameplay`. ABI C curada, wrappers C# escritos à mão no início, geração automática quando a superfície crescer. Hot reload por AssemblyLoadContext coletável. Sem scripts, a engine roda igual.

**Determinismo:** simulação em tick fixo de 60 Hz, render interpolado.

## 7. Capacidades

### Mundo

- Streaming por células, sem hitch acima de 16 ms.
- Visão de 1 km com HLOD.
- LOD automático por distância e por cobertura em tela.
- Terreno com clipmap, 4 km² na fatia.
- Vegetação instanciada, 100 mil instâncias.
- Céu procedural, ciclo dia/noite, clima.

### Cidade

- Quarteirões modulares de 100×100 m.
- Prédios com interior e streaming de interior.
- 200 NPCs com LOD de comportamento.
- 50 veículos simultâneos.
- Trânsito em grafo de ruas, faixas, semáforos e ultrapassagem.

### Personagem e câmera

- Cápsula com step, rampa, agachar, correr, pular, escalar e planar.
- Câmera em terceira pessoa, ombro, veículo e cinematográfica.
- Animação com blend 1D/2D, layers, IK de membros, retargeting e root motion.

### Render

- Deferred clustered, PBR metallic-roughness.
- Cel shading por material, com contorno.
- Sombras cascateadas com PCSS.
- TAA com velocity buffer e teste de imagem.
- Fog volumétrico, bloom, DOF, motion blur, exposição e color grading.
- SSR em superfícies molhadas.
- 1080p60 no hardware alvo.

### Áudio

- Mixer 3D, 64 vozes, oclusão, reverb por zona, ducking e streaming de música.

### Ferramentas

- Editor com viewport, hierarquia, inspetor, asset browser, console e profiler.
- Prefabs, undo/redo e multi-seleção.
- Play-in-editor com hot reload de C# e assets.
- Cook e build por linha de comando.

### UI de jogo

- Markup declarativo, estilo em folha e binding para view-models em C#.
- Layout por âncora, pilha, grade e scroll, em unidade independente de densidade.
- HUD, menus, diálogos, notificações, marcadores de mundo e minimapa.
- Localização por chave, acessibilidade e escala de UI.
- Editor visual com pré-visualização ao vivo.
- Orçamento: menos de 1 ms de CPU e de GPU por frame.

Detalhes em `docs/sistemas/UI.md`.

### Conteúdo

- glTF entra, o motor cozinha.
- LOD, colisão, UV de lightmap e navmesh automáticos.
- Um quarteirão autoral entra em horas, não em semanas.

## 8. Qualidade

- Todo módulo tem teste unitário.
- Todo sistema visual tem teste de imagem em WARP.
- Física e simulação determinísticas em tick fixo.
- Orçamento de frame: 16,6 ms de CPU e 16,6 ms de GPU.
- Build limpo com `/W4 /WX`.
- Nenhum sistema é considerado pronto sem estar exposto no editor e dentro do orçamento.

## 9. Riscos e provas

| Risco | Prova | Release |
|---|---|---|
| Hospedagem .NET e hot reload | Script roda, recarrega e não vaza | R1 |
| Estabilidade temporal | Imagem sem tremor em 300 frames | R2 |
| Streaming sem hitch | Travessia de 1 km com frame p99 abaixo de 16 ms | R3 |
| Multidão e trânsito | 200 NPCs e 50 carros a 60 fps | R5 |
| UI de produção | HUD e menus abaixo de 1 ms por frame | R6 |
| Pipeline de conteúdo | Quarteirão autoral em menos de 8 horas | R7 |

Se uma prova falhar, o sistema muda de abordagem. A engine não recomeça.

## 10. Releases

**R0 — Fundação.** Repositório, build, CI, core, plataforma, janela, input, log, math, jobs, harness de teste e modo headless.

**R1 — A engine que se usa.** Editor, cena, componentes, scripting em C#, play-in-editor e save/load. *Marco: criar, scriptar e jogar.*

**R2 — Imagem.** RHI, render graph, deferred, luz, sombra, TAA, pós, áudio base e testes de imagem. *Marco: uma cena estável e bonita.*

**R3 — Mundo.** Streaming, terreno, LOD/HLOD, vegetação, céu e clima. *Marco: travessia de 1 km sem hitch.*

**R4 — Personagem.** Controller, câmera, animação, IK e retarget. *Marco: um personagem que se move bem.*

**R5 — Cidade.** Prédios modulares, veículos, trânsito, multidão e navegação. *Marco: um quarteirão vivo.*

**R6 — UI de jogo.** Markup, layout, estilo, binding, input, animação, minimapa e editor visual. *Marco: HUD e menus de produção.*

**R7 — Conteúdo e ferramentas.** Pipeline completo, prefabs, profiler, cook e áudio de produção. *Marco: conteúdo em horas.*

**R8 — Fatia vertical.** Um distrito, 200 NPCs, 50 carros, 60 fps, polimento e endurecimento. *Marco: o primeiro jogo.*

## 11. Regra de mudança

Toda decisão tem ID e status:

- **FIXO** — só muda por RFC em `docs/rfcs/`, com problema, opções, custo, impacto e decisão. A mudança sobe a versão deste documento.
- **PROVISÓRIO** — muda com uma nota no histórico.

O objetivo não é congelar a engine. É impedir que uma decisão fechada seja reaberta por impulso no meio de uma release.

## 12. Histórico

- **0.1** — fundação do escopo.
- **0.2** — UI de jogo como sistema próprio e release R6; limite de arquivo; referência à arquitetura.
- **0.3** — limite por arquivo separado em `.cpp` e `.h`, com valor provisório e allowlist; revisão após a R2.
