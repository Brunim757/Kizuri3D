# Kizuri Engine — Roadmap de Desenvolvimento

**Autor:** Kizuri Studio
**Repositório:** `KizuriEngine` (público)

---

## Como ler este roadmap

As fases estão organizadas por **dependência técnica**, não por data — cada fase depende do que foi construído na anterior. Não há estimativa de tempo fixa (projeto solo, ritmo variável), mas a ordem deve ser respeitada: pular fases gera retrabalho.

Cada fase lista: **objetivo**, **entregáveis** e **critério de "pronto"**.

### Princípio: o Editor nasce cedo e cresce junto com cada sistema

Uma casca de Editor (Fase 3) nasce logo após o primeiro pixel ser renderizado (Fase 2), antes de qualquer sistema de gameplay. A partir daí, **toda fase que introduz um componente novo no ECS é obrigada a entregar, na mesma fase, o desenho daquele componente no Inspector** — nunca adiado para uma fase de "polimento" posterior (regra 7 do TDD).

A Fase 3 foi deliberadamente construída como uma fundação robusta (não um MVP raso), porque sistemas essenciais de UX de editor (Undo/Redo, seleção múltipla, notificações, autosave) são muito mais caros de retrofitar depois do que de construir certo desde o início — o mesmo raciocínio já aplicado aos riscos técnicos da seção 8 do TDD.

### Princípio de integração de bibliotecas de terceiros

**Regra explícita**: uma biblioteca de terceiros só é integrada na fase em que o sistema que a usa é realmente construído — nunca antes. Integrar sem uso real produz apenas um link vazio, não testado de verdade.

**Tabela de mapeamento — qual fase integra cada biblioteca da stack (TDD, seção 3):**

| Biblioteca | Fase de integração | Motivo |
|---|---|---|
| DirectXMath | Fase 1 | Núcleo — matemática usada por tudo desde o início |
| enkiTS | Fase 1 | Núcleo — Job System é fundação de todo o resto |
| Dear ImGui | Fase 3 | Editor Shell — nasce logo após o primeiro pixel renderizado |
| cgltf | Fase 4 | Usada pelo Pipeline de Assets |
| stb_image | Fase 4 | Usada pelo Pipeline de Assets |
| DirectXTex | Fase 4 | Requer wiring específico com DirectXMath (CONFIG) |
| FlatBuffers | Fase 4 | Formato de serialização de assets |
| zstd | Fase 4 (bootstrap já validado na Fase 0) | Compressão de pacote de assets |
| Jolt Physics | Fase 6 | Sistema de física |
| miniaudio | Fase 7 | Sistema de áudio |
| nethost / hostfxr | Fase 9 | Tooling .NET para hospedar o CoreCLR |
| Roslyn (Microsoft.CodeAnalysis.CSharp) | Fase 9 | Compilador C# usado pelo scripting |
| GameNetworkingSockets | Fase 15 | Rede P2P; toolchain pesado (OpenSSL/libsodium), isolado até esta fase |
| FreeType | Fase 18 ou 20 | Renderização de texto (UI de jogo e/ou editor) |
| Fork Awesome + IconFontCppHeaders | Fase 20 | Ícones do editor |

**Nota sobre a Fase 0**: libs single-header/simples de baixo risco foram todas *bootstrapadas* já na Fase 0 (CMake + chamada real de API via `KizuriHello`). `DirectXTex`, `GameNetworkingSockets`, `Roslyn`/`nethost` e os ícones seguem fora da Fase 0 pelos motivos já descritos na tabela.

---

## Status atual das fases

| Fase | Estado | Observação |
|---|---|---|
| 0 — Repositório | ✅ concluída | CI no GitHub Actions entrega artefato de build a cada push |
| 1 — Núcleo (C++) | ✅ concluída | Alocadores, Job System (enkiTS), janela/input, Origin Rebasing, RHI + backend DX11 |
| 2 — Renderer básico | ✅ concluída | Deferred + PBR + câmera livre + luz direcional. O renderer foi **muito além** desta fase: o `RendererList.md` está praticamente 100% (só restam N/A de DX11 e RT fora de escopo) |
| 3 — Editor Shell | ✅ concluída | Viewport, Hierarquia, Inspector, Console, Asset Browser, Settings; serialização `.kzscene`, autosave, Undo/Redo, seleção múltipla, notificações, menus de contexto, ImGuizmo |
| 4 — Pipeline de Assets | ✅ concluída | Import glTF, texturas, codecs internos, Asset Browser, dependências entre assets, upload assíncrono |
| 5 — Iluminação/sombras | ✅ concluída | CSM, PCF/PCSS, múltiplas luzes, clustered/Forward+, céu procedural, instancing, culling, FSR, Inspector de Light |
| 6 — Física (Jolt) | ✅ concluída | Jolt integrado, corpos/coliders, passo fixo 60Hz + interpolação, raycasts, ragdoll/joints, Inspector de Rigidbody, wireframe de colliders, toggle Simulate |
| 7 — Áudio + Mixer | 🔄 **em andamento** | Núcleo pronto: miniaudio (device + decoder), mixer com hierarquia de buses (Master/Música/SFX/Voz/Ambiente), volume/mute/solo, ducking entre buses, reverb (Freeverb) por bus, camada 3D posicional própria (atenuação linear/inverse/exponential + pan direcional), componente `AudioSource` no ECS com Inspector e painel "Audio Mixer" no Editor, mixer salvo como asset `.kzmixer`. Falta: clips no Asset Browser |
| 8 — Animação | ⏳ pendente | `Engine/Animation` ainda é stub |
| 9 — Scripting C# | ⏳ pendente | `Engine/Scripting` ainda é stub |
| 10–24 | ⏳ pendente | ECS/Gameplay, Prefabs, Scripting Visual, Câmera/Input, IA/Pathfinding, Rede, Save, VFX, UI de jogo, Level Design, Polimento, Docs, Launcher, Build, Vertical Slice |

**Onde a Fase 6 travou (e foi resolvido):** o teste de ragdoll derrubava o CI com `exit code 1` (access violation) — dois bugs reais no `Engine/Physics`:
1. *Double free* de constraint: no Jolt v5 `Constraint` é `RefTarget` e o `ConstraintManager` guarda `Array<Ref<Constraint>>`, logo `RemoveConstraint()` **já** libera o objeto; o `delete` manual do lado Kizuri destruía a mesma memória duas vezes. Agora o slot guarda `JPH::Ref<JPH::Constraint>` e quem controla a vida é o refcount do próprio Jolt.
2. *Use-after-free* no adaptador `Jolt::JobSystem → enkiTS`: tasks do enkiTS ainda pendentes na fila chamavam `ReleaseSlot()` num `JobSystemEnki` já destruído (o `WaitforAllAndShutdown()` do enkiTS executa essas tasks na thread principal). Agora o pool é compartilhado por `weak_ptr`, o destrutor drena os jobs em voo antes de liberar os slots, e o `PhysicsWorld` derruba o adaptador **antes** de destruir os corpos.

Lição operativa: **nunca chamar `delete` em objetos do Jolt v5** — o refcount do próprio Jolt é a fonte da verdade (`Ref<T>`).

---

## Fase 0 — Fundação do repositório

**Objetivo:** preparar o esqueleto do projeto antes de qualquer código de engine.

- Estrutura de pastas do repositório (`Engine/`, `Editor/`, `Launcher/`, `ThirdParty/`, `Samples/`)
- Configuração de build via GitHub Actions (compilação automática a cada push)
- **CI publica artefato de build baixável** (`actions/upload-artifact`) a cada execução — não só reporta verde/vermelho, entrega o binário compilado para o autor testar localmente (regras 5 e 8 do TDD); esse padrão vale para todo pipeline de CI daqui pra frente, não só a Fase 0
- Estrutura de solução C++ preparada para múltiplas `.lib` modulares desde o início
- Integração inicial de todas as bibliotecas de terceiros definidas no TDD (sem uso ainda, só linkando "hello world" de cada uma)

**Pronto quando:** o CI builda com sucesso um executável vazio que linka todas as `.lib` de terceiros, e esse executável fica disponível como artefato baixável na execução do workflow.

---

## Fase 1 — Núcleo de baixo nível (C++)

**Objetivo:** a fundação que tudo depende.

- Alocadores customizados (pool allocator, arena/stack allocator)
- Job System (integração do enkiTS)
- Janela e input via Win32 API nativo
- **Input crítico (esquiva, ataque) via polling de baixo nível (Raw Input/`GetAsyncKeyState`)**, fora do loop padrão de mensagens do Win32
- **Sistema de coordenadas com Origin Rebasing**: a origem do mundo é periodicamente reancorada perto do jogador, evitando erro de arredondamento de `float` 32-bit em mundos grandes
- RHI (Render Hardware Interface) — camada de abstração, ainda sem backend real
- Backend RHI para DirectX 11

**Pronto quando:** existe uma janela abrindo, um job rodando em paralelo à thread principal, e um `Clear()` de tela funcionando via RHI→DX11.

---

## Fase 2 — Renderer básico

**Objetivo:** primeira imagem 3D real na tela.

- Pipeline de Deferred Rendering
- **Cache de estado na RHI**: descarta chamadas de Render State já ativas na GPU
- Carregamento de mesh estática + material PBR simples
- Uma luz direcional funcional
- Câmera livre (debug, sem gameplay ainda)

**Pronto quando:** um modelo 3D importado aparece na tela, iluminado, com material PBR básico.

---

## Fase 3 — Editor Shell (fundação completa de UX)

**Objetivo:** dar ao dev uma forma robusta de ver e mexer no que já existe, antes de qualquer sistema de gameplay — incluindo os fundamentos de UX (Undo/Redo, seleção múltipla, notificações, autosave) que ficariam caros demais para adicionar depois.

**Painéis e infraestrutura base**
- Integração do Dear ImGui, tema dark
- Painéis: **Viewport**, **Hierarquia**, **Inspector** (com registro de "desenhadores" de componente, extensível), **Console**
- Gizmo de transformação (ImGuizmo) — modo Move, já que só existe Transform até aqui
- **Fila de edição no Inspector**: alterações da UI (Immediate Mode) são enfileiradas e aplicadas no início do frame seguinte, no mesmo ponto de sincronização do ECS
- Drag and drop básico (arrastar mesh já carregada para o Viewport cria entidade)
- Menu Bar inicial (File/Edit/Window/Help)
- **Menus de contexto (botão direito)**: Hierarquia (Criar Entity — sempre nasce só com Transform, sem redundância entre "vazio" e "cheio" já que adicionar qualquer outro componente é sempre um passo separado via Add Component ou drag-and-drop; Duplicar, Deletar, Renomear, Focar); Viewport (em objeto: Focar/Duplicar/Deletar; em espaço vazio: Criar Entity no ponto clicado); Inspector (Remover Componente, Resetar, Copiar/Colar valores) — cada painel novo em fases futuras ganha seu próprio menu de contexto na mesma entrega
- **Localização de shaders**: HLSL em pasta externa (`Shaders/`) durante o desenvolvimento da engine (hot-reload sem rebuild de C++); no `KizuriRuntime.exe` final (Fase 23), shaders são compilados para bytecode e embutidos, não distribuídos soltos

**Serialização de cena**
- **Serialização genérica baseada em reflection**: capacidade central de serializar qualquer componente para disco (`.kzscene`) e recarregar — "Salvar Cena" funciona de verdade desde aqui, não fica de fachada. Reaproveitada depois pela Fase 10 (snapshot de Play-in-Editor), Fase 11 (Prefabs) e Fase 16 (Save System do jogo) — mesma peça central, quatro consumidores
- **Autosave e recuperação de crash**: autosave periódico (intervalo configurável) para um arquivo de recuperação separado da cena principal; se o Editor abrir e encontrar uma recuperação mais recente que o último save, oferece restaurar
- **Indicador de alterações não salvas**: asterisco no título/aba da cena; prompt de confirmação ao fechar com mudanças pendentes

**Undo/Redo genérico**
- Sistema de Undo/Redo por **comando** (command pattern) e **diff incremental** (guarda só a mudança, não snapshot completo), cobrindo desde esta fase: criar/deletar entidade, adicionar/remover componente, editar valor no Inspector, mover/rotacionar/escalar via gizmo, parenting. A Fase 19 (Level Design) mais tarde **estende** este mesmo sistema para terreno/vegetação, em vez de implementar Undo/Redo do zero de novo

**Seleção múltipla**
- **Seleção por retângulo (rubber-band) no Viewport**: clicar e arrastar em espaço vazio desenha um retângulo; entidades cujo bounding box em tela intersecta o retângulo entram na seleção
- `Shift+clique` adiciona à seleção, `Ctrl+clique` alterna; mesma lógica na Hierarquia
- Gizmo em seleção múltipla: aparece na posição média do grupo; mover/rotacionar/escalar aplica o delta a cada entidade em relação à sua própria transform original (não colapsa todas num único pivô)

**Sistema de notificações (toasts)**
- Painel de notificações transitórias no canto da tela, com variantes de cor (erro/aviso/info/sucesso), auto-somem após alguns segundos, com um pequeno histórico recente acessível
- Usado sempre que uma ação não pode ser concluída (ex.: "Não é possível deletar: usado por 3 outros objetos", erro de build, falha de import) — complementa (não substitui) o sistema de validação contextual de componentes mal configurados (Fase 20)

**Pronto quando:** é possível criar entidades, selecionar várias por retângulo, mover o grupo pelo gizmo, desfazer/refazer qualquer uma dessas ações, salvar a cena (com indicador de não-salvo funcionando), e receber uma notificação clara quando uma ação for bloqueada — tudo sem editar código.

---

## Fase 4 — Pipeline de Assets

**Objetivo:** trazer conteúdo externo para dentro da engine de forma robusta.

- Import de glTF (cgltf)
- Import/compressão de texturas (stb_image + DirectXTex)
- Formato de asset interno via FlatBuffers (serialização própria, não glTF cru em runtime)
- Compressão de pacote de assets (zstd)
- **Upload assíncrono à GPU** via fila de staging buffers distribuída em vários frames
- **Texture Streaming por distância**: mip reduzido para objetos distantes
- **Asset Browser no Editor**: lista os assets importados; arrastar `.glb` para o Viewport cria entidade; arrastar para o campo Mesh no Inspector troca a malha (mapa caminho→índice, sem duplicar buffer na GPU)
- **Menu de contexto do Asset Browser**: Importar, Deletar, Renomear, Mostrar na pasta do sistema
- **Rastreamento de dependência entre assets**: cada asset mantém contagem de quem o referencia (ex.: um Material usando uma Textura); tentar deletar um asset em uso dispara uma notificação (Fase 3) listando os dependentes e bloqueia a exclusão, a menos que o dev confirme "forçar exclusão" — nesse caso, os dependentes passam a exibir um estado de "asset ausente" visível (não uma falha silenciosa)

**Pronto quando:** um `.glb` externo é importado, aparece no Asset Browser, pode ser arrastado para Viewport/Inspector, e tentar deletar um asset em uso gera aviso em vez de quebrar silenciosamente outra coisa.

---

## Fase 5 — Sombras e iluminação avançada

**Objetivo:** o visual "atmosférico" que o nicho souls-like exige.

- Cascaded Shadow Maps (CSM) para luz direcional
- Shadow maps para luzes pontuais/spot
- Suavização (PCF/PCSS)
- Múltiplas luzes dinâmicas simultâneas
- Integração do FSR (upscaling)
- Céu procedural atrelado à luz direcional (gradiente + sol), reconstruído via depth + matriz inversa de view-projection
- **Instancing agressivo** e **culling de oclusão obrigatório** — não é otimização a ser adicionada depois
- **Inspector**: desenhador de componente **Light** (tipo, cor, intensidade, raio)

**Pronto quando:** uma cena com 10+ luzes dinâmicas e sombras suaves roda sem queda perceptível de frame, e uma luz pode ser criada/ajustada inteiramente pelo Inspector.

---

## Fase 6 — Física (Jolt)

**Objetivo:** colisão e simulação física básica.

- Integração do Jolt Physics
- **Adaptador `Jolt::JobSystem` → enkiTS**: evita guerra de threads entre os dois schedulers
- Rigidbody, colliders (box, capsule, mesh)
- Raycasts
- Ragdoll e joints configuráveis
- **Interpolação entre passos de física** para evitar tremor visual
- **Ordem de cálculo em plataformas móveis** para evitar atravessar chão/teto
- **Inspector**: desenhador de **Rigidbody** e **Ragdoll/Joints**; visualização de colliders no Viewport (wireframe)

**Pronto quando:** um personagem cai por gravidade, colide com o cenário, um ragdoll reage a uma força, e um Rigidbody pode ser configurado inteiramente pelo Inspector.

---

## Fase 7 — Áudio (com mixagem completa)

**Objetivo:** som posicional funcional e uma cadeia de mixagem de nível profissional, não apenas tocar clipes.

- Integração do miniaudio
- Camada de áudio 3D posicional própria (atenuação por distância, direção)
- **Audio Mixer como asset próprio**: hierarquia de buses (Master → Música, SFX, Voz, Ambiente), cada bus com volume, mute e solo independentes
- **Ducking configurável**: um bus pode abaixar automaticamente o volume de outro quando ativo (ex.: diálogo abaixa a música)
- **Efeitos por bus**: zonas de reverberação (ex.: caverna, salão de boss) aplicadas ao bus correspondente
- **Editor de Audio Mixer**: painel dedicado, editando buses, volumes e roteamento visualmente
- **Inspector**: desenhador de **AudioSource** (clipe, volume, raio de atenuação, loop, **bus de saída** via dropdown)

**Pronto quando:** uma fonte sonora 3D muda de volume/direção com a câmera, pode ser roteada para um bus específico, o volume master/por-bus é ajustável pelo Editor de Mixer, e tudo isso é configurável sem código.

---

## Fase 8 — Animação (com retargeting)

**Objetivo:** o sistema mais crítico para o gênero souls-like — golpes, dashes, reações — e reaproveitamento de animação entre personagens diferentes.

- Sistema de esqueleto e skinning
- Blending de múltiplas camadas (locomotion + upper body + aim)
- IK de pés (ajuste ao terreno) e mãos
- Look-at de cabeça/tronco
- Root motion
- **Sincronização de hitbox com animação**: colliders de ataque atualizados no mesmo passo de animação, antes da física do frame
- **Retargeting de animação**: mapeamento de ossos do esqueleto de origem para um rig humanoide padrão, permitindo reaproveitar a mesma animação entre personagens com proporções/esqueletos diferentes
- **Inspector**: desenhador de **Animator** (esqueleto, clipe atual, mapeamento de retargeting); **Editor de state machine/blend tree**; preview de esqueleto/IK no Viewport

**Pronto quando:** um personagem anda sobre terreno irregular com IK correto, executa ataque com root motion preciso, a state machine é editável pelo Editor, e uma animação pode ser reaproveitada num segundo esqueleto via retargeting.

---

## Fase 9 — Scripting em C# (Roslyn)

**Objetivo:** abrir a engine para lógica de gameplay fora do C++.

- Hosting do .NET/CoreCLR dentro do executável (nethost/hostfxr)
- Pipeline de compilação via Roslyn
- **Fronteira C++/C# sem marshalling no hot path**: Blittable Types, ponteiro direto pro ECS, validado por teste automatizado no CI
- **Referências de Entity via handle com geração** (generational index)
- **API C# livre de alocação no hot path** (só `struct`/tipos value)
- **Hot-reload sem vazamento de memória**: `AssemblyLoadContext` colecionável, descarregado explicitamente
- API C# inicial expondo os sistemas já existentes
- Hot-reload de C# (recompilar sem reiniciar o Play Mode)
- **Inspector**: desenhador de **Script** — campos públicos editáveis (int, float, bool, referência de Entity)

**Pronto quando:** um script C# compila em runtime, afeta a cena sem reiniciar a engine, e seus campos públicos aparecem editáveis no Inspector.

---

## Fase 10 — ECS e Gameplay Framework

**Objetivo:** a camada que conecta o núcleo C++ à experiência do dev em C#.

- Estrutura ECS por baixo (cache-friendly, integrada ao Job System)
- **Escalonamento de leitura/escrita por componente**, evitando condição de corrida entre sistemas
- API C# que expõe o ECS como "componentes tradicionais"
- Sistema de Entity/parenting/hierarquia de cena
- **Play-in-Editor via snapshot em memória**, reaproveitando a serialização da Fase 3
- **Inspector/Hierarquia**: "Add Component" genérico (lista todos os componentes já registrados); parenting por drag-and-drop

**Pronto quando:** é possível criar uma Entity, adicionar qualquer componente via "Add Component", organizar em hierarquia por drag-and-drop, e testar com Play-in-Editor.

---

## Fase 11 — Sistema de Prefabs

**Objetivo:** permitir criar um "molde" de entidade reutilizável, instanciado várias vezes, com propagação de mudanças — essencial para popular um nível souls-like com inimigos e itens repetidos sem duplicar trabalho manualmente. Depende do ECS (Fase 10), da serialização genérica (Fase 3) e do Asset Browser (Fase 4).

- **Prefab como asset próprio** (`.kzprefab`), serializado com a mesma serialização por reflection já existente
- **Instâncias na cena referenciam o Prefab original**; alterar o Prefab propaga automaticamente para todas as instâncias
- **Overrides por instância**: uma instância pode sobrescrever propriedades específicas (ex.: vida maior, cor diferente) sem perder a ligação — overrides sobrevivem a atualizações do Prefab original
- **Apply/Revert**: uma instância pode "aplicar" sua mudança de volta ao Prefab (propaga a todas as outras instâncias) ou "reverter" (descarta o override, volta ao valor do Prefab)
- **Prefab Variants**: um Prefab pode ser baseado em outro (ex.: "Goblin Arqueiro" como variante de "Goblin Base"), herdando e podendo sobrescrever
- **Modo de edição isolado**: dar duplo clique num Prefab abre um modo de edição próprio, fora da cena, evitando editar sem querer apenas uma instância
- Instanciação via C# (`Instantiate(prefab)`) e via nó de Scripting Visual (`SpawnEntity`) — este último passa a existir formalmente aqui, já que precisa de um Prefab para instanciar
- **Hierarquia**: instâncias de Prefab aparecem com indicação visual distinta (ícone/cor), sinalizando a ligação com o asset original

**Pronto quando:** um Prefab de inimigo é criado, instanciado várias vezes numa cena, uma instância tem uma propriedade sobrescrita, e alterar o Prefab original propaga para as demais instâncias sem apagar aquele override.

---

## Fase 12 — Scripting Visual (nós)

**Objetivo:** o diferencial técnico central da Kizuri.

- Editor de nós (UI baseada em Dear ImGui)
- Pinos de execução (fio de fluxo) e pinos de dados (fio tipado)
- Geração de código C# real a partir do grafo
- **Nós assíncronos (Delay, loops com espera)**: máquina de estados assíncrona (`async`/`await`) ou corrotina gerenciada — nunca `Thread.Sleep`
- **Compilação incremental via Roslyn**: só o(s) grafo(s) modificado(s), em background pelo Job System
- **Exibição de erro de compilação**: dupla via Console (mensagem completa) e inline no nó problemático (ícone/borda vermelha diretamente no grafo, sem precisar caçar no log)
- Debug de grafo: valores em tempo real, breakpoints
- **Menu de contexto do editor de nós**: Adicionar Nó (busca por categoria), Deletar, Copiar/Colar nós
- **Inspector**: um grafo de nós salvo aparece no componente Script (Fase 9) como mais uma opção de script anexável

**Pronto quando:** um grafo simples (`OnBeginPlay → PlayAnimation`) funciona no Play Mode, um erro proposital no grafo aparece tanto no Console quanto destacado no nó culpado, e o grafo pode ser anexado a uma entidade como qualquer outro script.

---

## Fase 13 — Câmera e Input

**Objetivo:** os sistemas que todo jogo do gênero precisa, prontos de fábrica.

- Câmera terceira pessoa com colisão (C++ para o raycast, configurável via C#)
- Sistema de input com rebind completo e múltiplos dispositivos simultâneos
- **Inspector**: desenhador de **Camera Controller** (distância, sensibilidade, offset, alvo)

**Pronto quando:** a câmera segue um personagem sem atravessar paredes, o jogador remapeia uma tecla em runtime, e o Camera Controller é configurável pelo Inspector.

---

## Fase 14 — IA e Pathfinding

**Objetivo:** inimigos que se movem de forma inteligente pelo cenário.

- Geração de NavMesh
- **NavMesh particionado em regiões (tiles)**, recalculando só a região afetada
- Pathfinding hierárquico (mundos grandes)
- Group avoidance (múltiplos inimigos)
- **Inspector**: desenhador de **NavAgent**; visualização do NavMesh e do caminho calculado no Viewport

**Pronto quando:** um inimigo persegue o jogador desviando de obstáculos e outros inimigos, e o NavMesh é visualizável no Viewport.

---

## Fase 15 — Rede (P2P)

**Objetivo:** suporte a co-op no estilo Dark Souls.

- Integração do GameNetworkingSockets
- Modelo P2P com host migration
- **Client-side prediction com reconciliação (rollback)** — especificação técnica detalhada antes da implementação
- Sincronização básica de posição/estado de entidades entre peers
- **Inspector**: desenhador de **NetworkIdentity**

**Pronto quando:** dois clientes se conectam, veem um ao outro se movendo, a sessão sobrevive à saída do host original, e uma entidade é marcável como sincronizada pelo Inspector.

---

## Fase 16 — Save System (do jogo)

**Objetivo:** persistência de progresso do jogador — não confundir com "Salvar Cena" do Editor (Fase 3).

- **Reaproveita a serialização genérica da Fase 3** — constrói só a camada de gameplay: save slots, autosave, exposto via C# para o jogo decidir quando salvar
- **Inspector**: componente marcador **Saveable**

**Pronto quando:** o estado de uma cena é salvo num save slot e recarregado pelo jogo, e uma entidade é marcável como "salvável" pelo Inspector.

---

## Fase 17 — VFX

**Objetivo:** partículas de alta escala sem custo de CPU.

- Sistema de partículas GPU-based
- **Inspector/Editor**: desenhador de **Particle System** e painel de Editor de VFX (curvas de emissão, cor ao longo do tempo, forma de emissor)

**Pronto quando:** milhares de partículas rodam sem impacto perceptível de frame, e um efeito é montável do zero pelo Editor de VFX.

---

## Fase 18 — UI de jogo (HUD/menus)

**Objetivo:** ferramenta para o dev construir a interface do jogo final.

- Framework de UI retained-mode, separado do Dear ImGui (só do editor)
- Suporte a data-binding simples (ex.: barra de vida ligada a uma variável)

**Pronto quando:** um HUD simples (barra de vida + menu de pause) é montado visualmente, sem código extra.

---

## Fase 19 — Ferramentas de Level Design

**Objetivo:** construir níveis souls-like diretamente no editor.

- Terreno via sculpting manual (heightmap)
- Pintura de vegetação (instancing em massa)
- Editor de materiais PBR completo
- **Reaproveita o Undo/Redo genérico da Fase 3**, estendido para diffs de terreno/vegetação (não uma implementação nova)

**Pronto quando:** é possível esculpir um morro à mão e pintar vegetação, com Undo/Redo funcionando e boa performance.

---

## Fase 20 — Polimento do Editor

**Objetivo:** as ferramentas transversais que tornam o editor profissional — os desenhadores de componente e as fundações de UX (Undo/Redo, notificações, seleção múltipla) já nasceram junto de cada sistema (Fases 3 a 17); esta fase cruza tudo isso.

- Profiler visual (CPU/GPU/memória por sistema)
- Sistema de validação contextual (avisos estilo Godot) aplicado a todos os componentes existentes
- Atalhos de teclado customizáveis
- Menu Bar completo (File/Edit/Assets/Entity/Window/Help)
- Consolidação dos debuggers já existentes (física, animação, NavMesh) numa aba de Debug unificada

**Pronto quando:** todos os módulos acima estão integrados e acessíveis pela Menu Bar/atalhos.

---

## Fase 21 — Documentação integrada

**Objetivo:** documentação gerada junto com o código, navegável de dentro do editor.

- Geração de documentação a partir dos comentários XML do C#
- Painel de documentação dockable (estilo Godot), busca e `F1` contextual
- Publicação do mesmo conteúdo como site estático externo

**Pronto quando:** clicar `F1` sobre um nó/função abre a página de referência correta.

---

## Fase 22 — Launcher e Instalador

**Objetivo:** distribuição real da engine, fora do ambiente de desenvolvimento.

- Kizuri Launcher (lista de projetos, criação de projeto em branco, `.kzproj`)
- Instalador completo (Editor + Launcher + runtime .NET)
- Preferências de layout do editor salvas em `%APPDATA%/Kizuri/`, fora da pasta do projeto
- **Versionamento engine vs. projeto**: `.kzproj` guarda a versão exata da engine usada; se um projeto for aberto numa versão mais nova, o Launcher mostra um aviso não-bloqueante ("este projeto foi criado numa versão anterior; abrir pode causar incompatibilidades") e oferece fazer uma cópia de backup antes de prosseguir — nunca migra silenciosamente sem confirmação

**Pronto quando:** é possível instalar a engine numa máquina limpa, criar um projeto, abrir o Editor configurado, e abrir um projeto de versão antiga gera o aviso esperado em vez de falhar silenciosamente.

---

## Fase 23 — Sistema de Build final

**Objetivo:** gerar o executável do jogo, enxuto e funcional, sem exigir compilação C++ do dev.

- Compilação interna (só pelo Kizuri Studio) do núcleo como `KizuriRuntime.exe` — stub com os sistemas fundamentais já linkados
- **Configurações de build Debug e Release**: dois stubs pré-compilados pelo Kizuri Studio — Debug (asserts ativos, logs verbosos, símbolos de depuração, sem otimização agressiva) e Release (otimizado, logging mínimo) — o dev escolhe qual gerar no momento do Build, sem precisar compilar C++ para isso
- Análise de uso de API C# por projeto (quais sistemas opcionais o jogo referencia)
- Build 1-clique: copia o stub (Debug ou Release) renomeado para o nome do jogo, empacota assets + C# compilado, copia as `.dll` opcionais referenciadas

**Pronto quando:** dois projetos diferentes (com/sem rede) geram pastas de build de tamanhos diferentes, e o mesmo projeto pode gerar tanto uma build Debug quanto Release sem nenhuma compilação C++ na máquina do dev.

---

## Fase 24 — Vertical Slice de validação

**Objetivo:** provar que a engine cumpre a promessa de performance com conteúdo real.

- Construir uma cena pequena mas representativa de souls-like (boss simples, terreno esculpido, iluminação atmosférica, IA básica, alguns Prefabs de inimigo)
- Rodar benchmark nas GPUs de referência (mínimo e recomendado, definidas no TDD)
- Ajustar a tabela de requisitos de hardware com dados reais em vez de estimativa

**Pronto quando:** a vertical slice roda dentro da meta de frame time definida no TDD, no hardware mínimo estimado.

---

## Itens em aberto (ainda não decididos)

A auditoria anterior levantou 11 lacunas. Com as decisões desta rodada, restam as seguintes, genuinamente sem decisão ainda tomada:

### Streaming de mundo (fora de escopo, confirmado)
A arquitetura atual (Origin Rebasing, Fase 1) resolve precisão matemática em mundos grandes, mas conscientemente **não** inclui carregar/descarregar partes do mundo dinamicamente — isso só seria necessário se a engine expandir no futuro para um gênero de mundo aberto contínuo, fora do escopo souls-like atual. Decisão confirmada como fora de escopo, não uma lacuna pendente.

Todos os demais itens da auditoria anterior (Prefabs, Undo/Redo, seleção múltipla, dependência de assets, mixagem de áudio, retargeting, autosave, indicador de não-salvo, Build Debug/Release, erro de Roslyn, versionamento engine/projeto) foram resolvidos nesta rodada e já estão incorporados nas fases acima.

---

## Ordem de dependência resumida

```
Fase 0  → Repositório
Fase 1  → Núcleo (alocadores, job system, RHI, DX11, Origin Rebasing)
Fase 2  → Renderer básico
Fase 3  → Editor Shell completo (painéis, serialização/Salvar Cena, autosave,
           Undo/Redo genérico, seleção múltipla, notificações, menus de contexto)
Fase 4  → Pipeline de Assets + Asset Browser + dependência entre assets
Fase 5  → Iluminação/sombras avançadas + céu procedural + Inspector de Light
Fase 6  → Física (Jolt) + Inspector de Rigidbody/Ragdoll
Fase 7  → Áudio + Mixer completo (buses, ducking, reverb) + Inspector de AudioSource
Fase 8  → Animação + Retargeting + Inspector de Animator + editor de state machine
Fase 9  → Scripting C#/Roslyn + Inspector de Script
Fase 10 → ECS/Gameplay Framework + "Add Component" genérico + parenting
Fase 11 → Sistema de Prefabs (overrides, variants, apply/revert)
Fase 12 → Scripting Visual (nós) + exibição de erro inline
Fase 13 → Câmera/Input + Inspector de Camera Controller
Fase 14 → IA/Pathfinding + Inspector de NavAgent
Fase 15 → Rede P2P + Inspector de NetworkIdentity
Fase 16 → Save System do jogo (reaproveita serialização da Fase 3) + Inspector de Saveable
Fase 17 → VFX + Editor de partículas
Fase 18 → UI de jogo
Fase 19 → Ferramentas de Level Design (reaproveita Undo/Redo da Fase 3)
Fase 20 → Polimento do Editor (transversal)
Fase 21 → Documentação integrada
Fase 22 → Launcher/Instalador + versionamento engine/projeto
Fase 23 → Sistema de Build final + Debug/Release
Fase 24 → Vertical Slice de validação
```
