"""Hash a shader and its local include graph for generated-artifact validation."""
import hashlib
import pathlib
import re


def source_digest(source):
    source = pathlib.Path(source).resolve()
    digest = hashlib.sha256()
    active = set()

    def visit(path):
        path = path.resolve()
        if path in active:
            raise RuntimeError(f'Cyclic shader include: {path}')
        active.add(path)
        content = path.read_bytes().replace(b'\r\n', b'\n')
        digest.update(content)
        for name in re.findall(rb'^\s*#\s*include\s*"([^"\r\n]+)"', content, re.MULTILINE):
            # Framed dependency names avoid ambiguous concatenation and retain
            # existing stamps for shaders without includes.
            digest.update(b'\0include\0' + name + b'\0')
            visit(path.parent / name.decode('utf-8'))
            digest.update(b'\0endinclude\0')
        active.remove(path)

    visit(source)
    return digest.hexdigest()


if __name__ == '__main__':
    import sys
    print(source_digest(sys.argv[1]))
