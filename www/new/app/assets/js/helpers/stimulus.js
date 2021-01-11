function renderAction(controller, event, method) {
    return `${event}->${controller}#${method}`;
}

function renderTarget(controller, target) {
    return `${controller}.${target}`;
}

export { renderAction, renderTarget };
