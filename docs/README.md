# Space demo implementation handoff

Start with [HANDOFF.md](HANDOFF.md), then read the [PRD](PRD.md), [architecture](ARCHITECTURE.md), and [implementation tasks](TASKS.md). [DECISIONS.md](DECISIONS.md) records accepted direction and open decisions.

The application is implemented and renders on GTX 1080 Ti. PRD/architecture/workstream documents retain the original target design; they are not claims that every acceptance criterion is complete. See [VALIDATION.md](VALIDATION.md) and [HANDOFF.md](HANDOFF.md) for current evidence and remaining work.

| Document | Purpose |
| --- | --- |
| [PRD.md](PRD.md) | Intended experience, scope, quality targets, acceptance criteria |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Rendering design, ownership, build conventions, compatibility investigation |
| [TASKS.md](TASKS.md) | Ordered milestones and assignable work with dependencies and evidence |
| [DECISIONS.md](DECISIONS.md) | Accepted constraints, proposed choices, unresolved decisions |
| [SYSTEMS.md](SYSTEMS.md) | Generation, asset pipeline, geometry/LOD, shaders, camera and rendering contracts |
| [WORKSTREAMS.md](WORKSTREAMS.md) | Detailed subsystem assignments and parallel development plan |
| [HANDOFF.md](HANDOFF.md) | Session context and instructions for the coordinating agent |
| [CODE_STYLE.md](CODE_STYLE.md) | C++ conventions: language level, error handling, logging, ownership, settings scopes, layout |

Maintain task status and evidence in TASKS.md as work progresses. Record material changes to the design in DECISIONS.md; keep the PRD aligned with approved scope.

TASKS.md defines integration milestones. WORKSTREAMS.md decomposes those milestones into implementation assignments; it is the primary source for delegating subsystem work. Graphics compatibility gates GPU integration, not independent generation, asset, geometry, camera or shader-tooling development.
