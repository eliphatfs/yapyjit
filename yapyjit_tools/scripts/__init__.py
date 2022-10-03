def camel_to_snake(name: str):
    parts = []
    for ch in name:
        if ch.isupper():
            if len(parts):
                parts.append('_')
        parts.append(ch.lower())
    return ''.join(parts)
