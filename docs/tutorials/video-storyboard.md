> [中文](video-storyboard.md) | **English**

# Video Tutorial Series Scripts

An early deliverable for ROADMAP M8.2 "Video Tutorials": six-episode storyboard scripts plus verbatim narration (meant to be read as-is). Actual filming requires operating the GUI and a browser in a real Windows environment and is done by a human; this document guarantees that the recorder can hit "record" without needing to understand the product any further.

## Series Conventions

- Audience: users encountering Wingman for the first time (no assumption of Go/C++ development skills).
- Each episode is ≤ 8 minutes and covers one main thread only; botched operations are cut directly, never re-recorded from a rollback.
- Narration uses short declarative sentences with no marketing speak; terminology matches docs/guide (Runtime / GUI / orchestrator / Agent / trigger / macro).
- Recording spec: 1920×1080, 60fps; mouse movement slowed down, dwell ≥1s on click points; 100% zoom throughout.
- Uniform intro: logo 3s + this episode's title; uniform outro: next-episode teaser 5s + documentation link subtitle.
- File naming: `ep01-install-first-run.mp4` (lowercase hyphens); subtitles are published together with the video as SRT.
- Demo scripts must always be existing files under examples/lua_scripts/ (hello.lua, color_detect.lua, etc.); do not hand-type new scripts while recording.

## Episode Overview

| Episode | File | Main topic | Related docs | Target duration |
|---------|------|-----------|--------------|-----------------|
| 1 | ep01-install-first-run | Install, launch, run the first script end to end | docs/guide/getting-started.md | 6 min |
| 2 | ep02-runtime-gui-tour | Features of each Runtime GUI page | docs/guide/runtime-gui.md | 7 min |
| 3 | ep03-connect-orchestrator | Agent registers outbound to the orchestrator | docs/guide/getting-started.md ("Run Modes") | 5 min |
| 4 | ep04-first-script | Pixel detection with an example script | docs/guide/script-development.md | 6 min |
| 5 | ep05-triggers-macros | Trigger configuration and macro record/playback | docs/guide/runtime-gui.md, docs/guides/triggers.md | 7 min |
| 6 | ep06-dashboard-remote | Dashboard script dispatch and remote takeover | docs/guide/dashboard.md | 7 min |

## EP01 Install and First Launch

Prerequisites: a clean Windows 10/11 VM, with the Release artifacts downloaded (one installer package and one portable ZIP).

| # | Visual | Operation | Narration (verbatim) |
|---|--------|-----------|---------------------|
| 1 | Desktop | Open the Releases page | "Wingman ships in two distribution forms: an installer and a portable version." |
| 2 | Downloads folder | Double-click the InnoSetup installer, click through with the defaults | "The installer installs into Program Files; the portable version is ready to run after extraction." |
| 3 | Start menu | Launch Runtime | "Runtime is the local executor; on first launch it generates a default agent.toml." |
| 4 | Runtime logs | Point out the outbound connection attempt logs | "Runtime only connects outward to the orchestrator; it opens no control port locally." |
| 5 | Command line | `wingman-agent.exe script examples\lua_scripts\hello.lua` | "The command line can run Lua scripts directly, no orchestrator needed." |
| 6 | Output window | Show the screen resolution output | "hello.lua calls the built-in screen module and prints the current screen resolution." |
| 7 | Portable ZIP | Extract to drive D: and run the same script | "The portable version behaves identically to the installer and suits carrying on a USB drive." |

## EP02 Runtime GUI Overview

Prerequisites: the EP01 environment; the GUI ships with the release package as a Tauri frontend.

| # | Visual | Operation | Narration (verbatim) |
|---|--------|-----------|---------------------|
| 1 | GUI launch | Launch the GUI, wait for the IPC connection badge to turn green | "The GUI controls Runtime through local IPC; it does not go over the network." |
| 2 | Dashboard page | Point out the connection status and runtime statistics | "The dashboard shows the connection state between Runtime and the orchestrator." |
| 3 | Scripts page | Start and stop a script | "Script states come in three kinds: running, stopped, and error." |
| 4 | Scripts page | Point out the script editing entry | "For editing, VS Code with EmmyLua is recommended; the GUI only starts and monitors." |
| 5 | Triggers page | Open the creation panel, without saving | "Triggers are demonstrated in detail in Episode 5." |
| 6 | Macros page | Record 3 seconds of mouse operations and play back | "Macro playback replays the recorded sequence; the speed and repeat count are adjustable." |
| 7 | Screen page | Full-screen capture | "The screen tools support full-screen capture, region selection, and color picking." |

## EP03 Connecting to the Remote Orchestrator

Prerequisites: the orchestrator is deployed (Server + Dashboard), and an account has been created.

| # | Visual | Operation | Narration (verbatim) |
|---|--------|-----------|---------------------|
| 1 | agent.toml | Edit the server address and token | "agent.toml points at the orchestrator address; the registration token is generated in the orchestrator backend." |
| 2 | Runtime restart | After launch, look for the registration-success logs | "Once registration succeeds, this machine shows up in the orchestrator's Agent list." |
| 3 | Dashboard | Refresh the Agent list, check the tags | "Results of operations dispatched by the orchestrator come back over this outbound connection." |
| 4 | Network panel | Disconnect the network, watch the state change, then restore it | "If the connection drops, Runtime reconnects automatically; local scripts are unaffected." |

## EP04 Pixel Detection with an Example Script

Prerequisites: the EP02 environment; the example scripts ship with the repository.

| # | Visual | Operation | Narration (verbatim) |
|---|--------|-----------|---------------------|
| 1 | File manager | Open examples/lua_scripts/color_detect.lua | "The example scripts all live in the lua_scripts directory; this episode uses color detection." |
| 2 | Editor | Point out the targetColor and region parameters | "The script looks for red pixels inside a region; the region can be specified with screen coordinates." |
| 3 | GUI Scripts | Put the script in the scripts directory, select it on the Scripts page to start | "Once the script is in the scripts directory, the Scripts page can start it directly." |
| 4 | Output window | Show the findColor result and the number of matches | "This demo uses the full screen for simplicity; in practice smaller is faster, so don't use the full screen as the region." |
| 5 | Scripts page | Stop the script, point out the state returning to gray | "After stopping, the state returns to stopped, and the logs are kept." |

## EP05 Triggers and Macros

Prerequisites: the EP04 environment.

| # | Visual | Operation | Narration (verbatim) |
|---|--------|-----------|---------------------|
| 1 | Triggers page | Create a new trigger, choose time_elapsed as the condition | "The time trigger fires at intervals, used to execute operations periodically." |
| 2 | Triggers page | Choose run_script as the action, specify the script | "Actions can be running a script, clicking, key presses, delays, or log output." |
| 3 | Triggers page | Fill in the cooldown and save | "The cooldown prevents repeated execution when the condition keeps hitting." |
| 4 | Triggers page | Create a color_found trigger | "A color trigger only takes effect when the target color is detected within the specified region." |
| 5 | Triggers page | Disable a trigger, then look at the trigger records | "Disabling does not delete the configuration; the trigger history remains queryable in the records." |
| 6 | Macros page | Record a mouse-and-keyboard flow and save it | "Macros suit one-off flows; scripts suit automation that needs long-term maintenance." |
| 7 | Macros page | Play back, change the speed from 100% to 300%, play back again | "Playback speed is adjustable between 10% and 500%; the repeat count from 1 to 100." |

## EP06 Dashboard and Remote Takeover

Prerequisites: the EP03 orchestrator plus two machines (A = operator side, B = controlled side).

| # | Visual | Operation | Narration (verbatim) |
|---|--------|-----------|---------------------|
| 1 | Dashboard | Dispatch scripts in batches to B from the orchestrator | "The orchestrator can execute scripts in batches by tag." |
| 2 | Remote desktop | Request a ticket to take over B | "Takeover requires the desktop:control permission; view-only needs just view." |
| 3 | Remote desktop | Operate B inside the takeover window | "The pixel stream is translated by guacd; watch it directly in the browser, no client install needed." |
| 4 | File transfer | Upload a file within the takeover session | "File transfer goes through virtual drive mapping, and operations are audited." |
| 5 | Audit reports | Open the session-records page | "Every takeover produces a session record that can be reviewed later." |

## Recording and Release Checklist

- [ ] After each episode, self-check first: whether narration and visuals are in sync, whether there are misstatements, and whether sensitive information (tokens, internal addresses) is masked.
- [ ] SRT subtitles share the video's filename, authored and verified per srt-template.md; terminology spelling matches docs/guide.
- [ ] After uploading, add the link next to the File column in the episode overview table.
- [ ] Once all episodes are recorded and published, check off ROADMAP M8.2 "Video Tutorials".
