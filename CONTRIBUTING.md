# Contributing

There is no code yet, so there is nothing to build, nothing to test, and no style guide to follow. What there is, is a document making claims about how the physical world works. Those claims can be wrong, and finding out that they are is currently the whole job.

## The one rule

**An uncited number is a bug.**

Every physical quantity in this project carries a `source`. Reduction temperature of hematite: cite it. Settling velocity of kaolinite: cite it. Density of ilmenite: cite it. Numbers we have not yet sourced ship as `source: UNVERIFIED`, and they are tracked issues, not facts.

This is not pedantry. It is the difference between a project a metallurgy student can use and a project that merely says the word "realism" a lot. It also means **we cannot quietly balance a number** — to change one you have to change its citation, and there isn't one, because reality does not negotiate.

Several `UNVERIFIED` numbers currently sit in `DESIGN.md`, written from a language model's memory. Replacing one with a source is the most valuable thing anyone can do here today.

## What is most useful, in order

1. **A number we got wrong, with a source.**
2. **A process no craftsman would recognise.** If you have actually run a bloomery, thrown a pot, panned a stream, or worked in a mill, you know things that are not written down where we can find them. Your hands are the citation; you do not need a paper.
3. **A reason the grade/recovery mechanic will be tedious rather than interesting.** This is the risk most likely to kill the project, and it is the one we are worst positioned to see.
4. **Prior art we missed.** We would rather learn something exists than build it twice. See the README's three claims — if one of them is already done, tell us today.
5. **An argument that the whole thing is misconceived.**

## Why #5 is on that list

This project was designed by one human and one language model over a long evening. A language model is optimised to be agreeable: it will find the merit in nearly any direction it is pointed. That means mutual enthusiasm between the two of us is not evidence the idea is good — it is evidence of nothing, and from the inside it is indistinguishable from the real thing.

That failure cannot be corrected from inside the conversation. It is Risk #5 in `DESIGN.md`, and it is the dangerous one precisely because it feels good.

So hostility is welcome, and a blank issue saying **"you two are lame"** is a legitimate contribution. It is the only signal here that neither author can manufacture.

## Tone

Be as blunt as the argument requires. Do not be cruel to people. The document is fair game.

## Licence

Contributions to documentation and data are under [CC-BY-4.0](LICENSE-docs); contributions to code, when code exists, are under [Apache-2.0](LICENSE). By opening a pull request you agree your contribution ships under those terms.
