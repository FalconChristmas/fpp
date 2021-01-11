function renderClasses(block, element = '', modifiers = []) {
    let cssClass = block;

    if (element) {
        cssClass = `${cssClass}__${element}`;
    }

    if (modifiers.length === 0) {
        return cssClass;
    }

    return modifiers.map((modifier) => {
        return `${cssClass}--${modifier}`;
    }).join(' ');
}

export { renderClasses };
