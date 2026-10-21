import { existsSync } from 'node:fs';
import { copyFile, mkdir, readFile, writeFile } from 'node:fs/promises';
import { dirname, join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';
import { build, transform } from 'esbuild';

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const source = join(root, 'web');
const output = join(root, 'data');
const staticFiles = ['index.html', 'app.js', 'style.css', 'gpio-pins.json'];

async function bootstrap() {
  if (existsSync(source)) return;
  await mkdir(source, { recursive: true });
  for (const name of staticFiles) {
    if (existsSync(join(output, name))) {
      await copyFile(join(output, name), join(source, name));
    }
  }
  console.log('Created web/ from current data/ assets. Format sources in web/ before editing.');
}

async function buildWeb() {
  const args = process.argv.slice(2);
  if (args.some((arg) => arg !== '--bootstrap')) {
    throw new Error('Usage: npm run build -- [--bootstrap]');
  }
  if (args.includes('--bootstrap')) await bootstrap();
  if (!existsSync(source)) {
    throw new Error('web/ does not exist. Run npm run build -- --bootstrap once.');
  }

  const assets = new Map();
  for (const name of staticFiles) {
    const path = join(source, name);
    let text = await readFile(path, 'utf8');
    if (name.endsWith('.html')) {
      text = text.replace(/<!--(?!\[if)[\s\S]*?-->/g, '').replace(/>\s+</g, '><').trim();
    } else if (name.endsWith('.css')) {
      text = (await transform(text, { loader: 'css', minify: true, legalComments: 'none' })).code;
    } else if (name.endsWith('.js')) {
      const result = await build({
        absWorkingDir: root,
        entryPoints: [path],
        bundle: true,
        minify: true,
        format: 'iife',
        platform: 'browser',
        target: ['es2020'],
        write: false,
        legalComments: 'none'
      });
      text = result.outputFiles[0].text;
    } else if (name.endsWith('.json')) {
      text = JSON.stringify(JSON.parse(text));
    }
    assets.set(name, text);
  }

  const localConfig = join(source, 'config.local.json');
  const configSource = existsSync(localConfig) ? localConfig : join(source, 'config.example.json');
  assets.set('config.json', JSON.stringify(JSON.parse(await readFile(configSource, 'utf8'))));

  await mkdir(output, { recursive: true });
  for (const [name, text] of assets) {
    await writeFile(join(output, name), text, 'utf8');
    console.log(`Built data/${name}: ${Buffer.byteLength(text, 'utf8')} bytes`);
  }
  console.log(`Config source: ${relative(root, configSource)}`);
}

buildWeb().catch((error) => {
  console.error(`Web build failed: ${error.message}`);
  process.exitCode = 1;
});
