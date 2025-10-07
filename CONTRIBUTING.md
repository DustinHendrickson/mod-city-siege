# Contributing to City Siege Module

Thank you for your interest in contributing to the City Siege module!

## How to Contribute

### Reporting Bugs

If you find a bug, please create an issue with:
- Clear description of the bug
- Steps to reproduce
- Expected behavior
- Actual behavior
- Server configuration (AzerothCore version, module config)
- Any relevant log entries

### Suggesting Features

Feature suggestions are welcome! Please:
- Search existing issues first to avoid duplicates
- Clearly describe the feature and its benefits
- Explain how it fits with the module's purpose
- Provide examples if possible

### Pull Requests

We welcome code contributions! Please follow these guidelines:

#### Before You Start

1. Fork the repository
2. Create a new branch for your feature: `git checkout -b feature/my-feature`
3. Check existing issues and PRs to avoid duplicate work

#### Coding Standards

Follow AzerothCore's coding standards:

1. **Formatting**
   - Indent with 4 spaces (no tabs)
   - Opening braces on the same line
   - Follow existing code style

2. **Naming Conventions**
   - Variables: `camelCase`
   - Functions: `PascalCase`
   - Constants: `UPPER_SNAKE_CASE`
   - Private members: `m_variableName`

3. **Comments**
   - Use `/** */` for function documentation
   - Use `//` for inline comments
   - Document complex logic
   - Explain "why" not just "what"

4. **Example**
   ```cpp
   /**
    * @brief Brief description of the function.
    * @param paramName Description of parameter.
    * @return Description of return value.
    */
   void MyFunction(uint32 paramName)
   {
       // Comment explaining complex logic
       if (condition)
       {
           DoSomething();
       }
   }
   ```

#### File Headers

All source files must include the license header:

```cpp
/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
```

#### Testing

Before submitting:
1. Compile the module without errors
2. Test your changes in-game
3. Verify no regressions in existing functionality
4. Test with various configuration options
5. Check debug logs for errors

#### Commit Messages

Write clear, descriptive commit messages:

```
Short (50 chars or less) summary

More detailed explanatory text, if necessary. Wrap it to about 72
characters or so. The blank line separating the summary from the body
is critical.

Further paragraphs come after blank lines.

- Bullet points are okay, too
- Use a hyphen or asterisk for the bullet
```

Examples:
- ✅ `feat: Add configurable spawn delay for siege events`
- ✅ `fix: Correct creature despawn on event end`
- ✅ `docs: Update README with new configuration options`
- ❌ `Fixed stuff`
- ❌ `Update`

#### Pull Request Process

1. Update documentation (README.md) if needed
2. Update SUMMARY.md with implementation status
3. Add your name to AUTHORS file
4. Ensure code compiles and runs
5. Create PR with clear description:
   - What does this PR do?
   - Why is this change needed?
   - How has it been tested?
   - Any breaking changes?

### Documentation

Improvements to documentation are always welcome:
- Fix typos or unclear instructions
- Add examples
- Improve configuration guides
- Translate documentation

### Development Priorities

Current priorities for contributions:

1. **High Priority**
   - Creature spawning implementation
   - AI configuration
   - Creature text system
   - Reward distribution

2. **Medium Priority**
   - Additional configuration options
   - Performance optimizations
   - More spawn locations
   - Custom creature templates

3. **Nice to Have**
   - Achievement system
   - Quest integration
   - Statistics tracking
   - Web interface for monitoring

## Code of Conduct

- Be respectful and constructive
- Welcome newcomers and help them learn
- Focus on what is best for the community
- Show empathy towards other contributors

## Questions?

If you have questions:
- Check existing issues and documentation
- Ask on AzerothCore Discord
- Create a discussion issue (not a bug report)

## License

By contributing, you agree that your contributions will be licensed under the GNU AGPL v3 license.

## Recognition

Contributors will be added to the AUTHORS file and recognized in release notes.

Thank you for contributing to the City Siege module!
