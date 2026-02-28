const { execSync } = require('child_process');
const path = require('path');

const workDir = 'C:\\Users\\Sarah\\Documents\\Unreal Projects\\TheWytching';
const options = { cwd: workDir, encoding: 'utf-8' };

const commands = [
  { name: 'git status', cmd: 'git status' },
  { name: 'git log --oneline -20', cmd: 'git log --oneline -20' },
  { name: 'git reflog --oneline -10', cmd: 'git reflog --oneline -10' },
  { name: 'git diff --stat HEAD', cmd: 'git diff --stat HEAD' },
  { name: 'git show --stat e16259bae9d6cfceded18e15f31b0a00ee174488', cmd: 'git show --stat e16259bae9d6cfceded18e15f31b0a00ee174488' },
  { name: 'git ls-tree --name-only e16259bae9d6cfceded18e15f31b0a00ee174488', cmd: 'git ls-tree --name-only e16259bae9d6cfceded18e15f31b0a00ee174488' }
];

console.log(`\n=== Running git commands in: ${workDir} ===\n`);

commands.forEach(({ name, cmd }) => {
  console.log(`\n>>> ${name}`);
  console.log('─'.repeat(80));
  try {
    const output = execSync(cmd, options);
    console.log(output);
  } catch (error) {
    console.error(`ERROR: ${error.message}`);
    if (error.stdout) console.log('STDOUT:', error.stdout);
    if (error.stderr) console.log('STDERR:', error.stderr);
  }
});

console.log('\n=== All commands completed ===\n');
