# HTB/CTF Tutor

A manual approach and a Claude skill designed to help you learn and improve your penetration testing and CTF skills through AI-assisted tutoring while working on retired HackTheBox and other capture-the-flag challenges.

## The Idea: AI Assistants as Learning Tutors

When working through HackTheBox machines and CTF challenges, it's easy to get stuck or frustrated, and looking at the writeup can get you unstuck, but also spoils the step (and maybe more). However, these challenges represent incredible learning opportunities. Rather than simply looking up writeups or spoilers, using an AI assistant as a tutor can help you by using AI as a teaching tool rather than a solution provider - getting nudges in the right direction rather than complete answers.

I'm by no means the first person to think about this. Shoutout to ToxSec who made [this post](https://bsky.app/profile/toxsec.bsky.social/post/3lz5phdshb22r) about it, as well as others.

## Manual Free Way

You can use this approach with any AI chatbot (Claude, ChatGPT, etc.) by providing the following prompt:

```
You are an expert penetration tester and CTF player serving as my tutor. I'm working on a HackTheBox machine / CTF challenge and need guidance. I'll give you a writeup URL to study.

Your role is to:

- Help me learn by giving hints, not direct answers
- Ask me questions about what I've tried and what I'm thinking. Do not ask questions that don't advance me towards the path of the solution in the writeup.
- Explain concepts and techniques when I'm stuck on fundamentals
- Suggest next steps or areas to investigate without spoiling the solution
- Do not provide information from the writeup that I haven't given you already.
- Only ask the most useful one or two questions at a time.

Please guide me as a mentor would, helping me develop my skills rather than just solving it for me. Do not under and circumstances leak information from the writeup that the user hasn't provided or directly give information. Don't give hints or assign tasks. Just ask questions to get the user on the correct path.

Solution Writeup URL: [URL]
What I've tried: [DESCRIBE YOUR ATTEMPTS]
Where I'm stuck: [DESCRIBE YOUR PROBLEM]
```

Simply copy this prompt, fill in your specific situation, and paste it into any AI chatbot to get started.

## Claude Skill

This repository provides a specialized Claude skill that enhances the tutoring experience with additional context and capabilities optimized for security challenges.

### Installation

#### Claude.ai

1. Download the latest `.skill` file from the `claude-skill` folder in this repo
2. In Claude.ai, click on your profile icon
3. Select "Settings" → "Skills"
4. Click "Import Skill" and select the downloaded file
5. Make sure the new htb-mentor skill is active in the menu

#### Claude Code

1. Download the latest `.skill` file from the `claude-skill` folder in this repo
2. Extract into the appropriate `skills` directory:

**Personal Skills** (available in all projects):
```bash
# Extract to your personal skills directory
mkdir -p ~/.claude/skills/
unzip htb-mentor-fixed.zip -d ~/.claude/skills/
```

**Project Skills** (specific project only):
```bash
# In your project directory
mkdir -p .claude/skills/
unzip htb-mentor-fixed.zip -d .claude/skills/
```

### Usage

Once installed:

1. Start a new conversation with Claude
2. Tell Claude about your challenge and what you're working on
3. Follow the guided tutoring approach to work through the challenge

The skill works best when you:

- Clearly describe what you've already tried
- Share relevant command outputs or observations
- Ask specific questions about concepts you don't understand
- Request hints rather than direct answers initially

## Contributing

Found a way to improve the tutoring approach? Have suggestions for better hints or explanations? Contributions are welcome!

## License

MIT

------

**Remember:** The goal is to learn and improve your skills. Resist the temptation to ask for direct answers - the struggle is where the learning happens!
