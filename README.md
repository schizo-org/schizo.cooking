# Marker

Worst markdown parser on the face of earth, written in C because I enjoy
shooting myself in the foot. Designed with a user in mind, but not for yours.

Please do _not_ try to use this in an actual production scenario, but you are
welcome to mess around with it and submit PRs to improve functionality.

## Features

We can somewhat reliably parse the following:

- Bold: **text**
- Italic: _text_
- Inline code: `code`
- Links: `[text](url)`
- Escape characters: \*, \_, \#, etc.
- Blockquotes: `> text`

## Testing

A very basic test is included, but it will not assert anything. Please review
thee HTML output manually.
