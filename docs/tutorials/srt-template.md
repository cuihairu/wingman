> [中文](../zh/tutorials/srt-template.md) | **English**

# Subtitle (SRT) Authoring Template

Companion to the "publish subtitles with the video as SRT" line in
[video-storyboard.en.md](video-storyboard.md). The storyboard already carries
the verbatim narration, so subtitle text needs no writing — after recording only
timecode alignment remains. This template fixes where the text comes from, how to
split, how to fill timecodes, and how to verify.

## File conventions

- Same name and directory as the video: `ep01-install-first-run.srt` with
  `ep01-install-first-run.mp4`.
- UTF-8 without BOM, LF line endings, no trailing whitespace.
- Standard SRT layout: cue number, `start --> end`, text, blank line between cues.
- One SRT per video; no multi-language tracks.

## Cue text: copied verbatim from the storyboard, never rewritten

- One table per episode; the "Narration (verbatim)" column in shot order is the
  cue order, numbered from 1.
- Text is unchanged: no filler words, no abbreviations, no merging two shots'
  narration.
- Keep the quotation marks and English terms exactly as written (full-width
  Chinese quotation marks).

Cue counts per episode (= storyboard rows): EP01 7, EP02 7, EP03 4, EP04 5,
EP05 7, EP06 5 — 35 total. Count them; one short means one dropped.

## Splitting rules (per-cue duration)

- Target: a cue lasts ≤ 4s, ≤ 40 Chinese characters. Most storyboard lines are
  already within that, so usually no splitting is needed.
- Split long ones at a comma; the break must not fall inside a word, and the two
  parts must still cover that one shot's picture.
- Never split across shots: a cue's span stays inside its shot.

## Timecodes

Format `00:MM:SS,mmm --> 00:MM:SS,mmm` (two-digit hours, comma for
milliseconds).

| Rule | Value |
|------|-------|
| Start | the picture frame where the shot's narration audio begins |
| End | the moment before the shot's picture ends |
| Intro | first cue no earlier than the end of the intro (logo 3s + episode title) |
| Outro | last cue ends before the outro (next-episode teaser 5s + docs-link caption) |
| Gaps | no overlap; ≥300ms between adjacent cues |
| Precision | adjust the millisecond field to the picture; don't round to whole seconds |

## EP01 example (placeholder timecodes)

The timecodes below are sequential placeholders ("after the 3s intro"), shown
only to demonstrate format and text. **Replace every one against the actual
footage after recording.**

```
1
00:00:03,000 --> 00:00:07,000
「Wingman 提供两种发行形态：安装包和便携版。」

2
00:00:07,400 --> 00:00:11,400
「安装包装到 Program Files；便携版解压即用。」

3
00:00:11,800 --> 00:00:15,800
「Runtime 是本机执行器，首次启动会生成默认 agent.toml。」

4
00:00:16,200 --> 00:00:20,200
「Runtime 只主动向外连接中控，本机不开控制端口。」

5
00:00:20,600 --> 00:00:24,600
「命令行可以直接跑 Lua 脚本，不需要中控。」

6
00:00:25,000 --> 00:00:29,000
「hello.lua 调了内置 screen 模块，打印当前屏幕分辨率。」

7
00:00:29,400 --> 00:00:33,400
「便携版与安装版行为一致，适合 U 盘携带。」
```

## Authoring flow

1. Export the cue text from the storyboard (shot order = cue number); write the
   placeholder-only SRT first.
2. Record, then align each cue's start and end in a player and edit timecodes
   one by one.
3. Verify: cue count matches the storyboard; no overlap, no empty cues; first
   and last cues sit outside intro/outro.
4. Terminology pass: compare spelling word by word against the docs/guide prose
   (Runtime, GUI, orchestrator, Agent, trigger, macro).
5. Deliver: upload the SRT alongside the mp4 under the same name and backfill
   the link into the storyboard's episode overview table.
