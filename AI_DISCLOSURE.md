# AI Development Disclosure

Orbit Wallpaper Engine was developed with substantial generative-AI assistance.

This document is intentionally explicit so users and contributors can understand how the project was produced.

## Human contribution

The human project owner drove the project and retained decision-making responsibility. Human contribution included:

- originating the wallpaper-engine concept and defining its purpose;
- defining the visual direction and desired interaction behaviour;
- deciding the Orbit integration model and later standalone-release requirements;
- selecting features, naming, configuration behaviour and compatibility goals;
- identifying bugs and describing expected behaviour from real desktop use;
- repeatedly building and running the software on the target system;
- manually testing renderer startup, animation, shader compatibility, palette behaviour, settings UI, service migration, installer behaviour and regressions;
- reviewing proposed changes and accepting, rejecting or refining them;
- making the release-policy decisions around shader attribution, undeclared licences and blacklisting.

The bundled `wave.frag` was developed specifically for this project through the same iterative, human-directed and AI-assisted process.

## AI-assisted contribution

OpenAI ChatGPT was used extensively as an implementation and debugging assistant.

AI-assisted work included substantial drafting and revision of:

- C renderer code and shader-compatibility logic;
- QML settings UI;
- Python helper/catalogue code;
- shell installer, migration and cleanup scripts;
- systemd integration;
- debugging hypotheses and code patches;
- repository cleanup and packaging work;
- documentation, migration notes, policy text and release preparation.

Some generated patches failed or required correction during development. Changes were iteratively tested and revised rather than being accepted solely because they were AI-generated.

## Responsibility

AI output is not treated as an independent author, maintainer or rights holder.

The project maintainer is responsible for reviewing what is published, responding to bug and rights-holder reports, and deciding whether AI-assisted changes are accepted into the repository.

Contributors should not assume that an existing implementation is correct merely because it was generated or reviewed with AI assistance. Normal code review, testing, security review and licensing review remain appropriate.
