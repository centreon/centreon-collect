import fs from 'fs/promises';



export const getConfig = async (): Promise<JSON> => {
    return JSON.parse((await fs.readFile('/etc/centreon-broker/central-broker.json')).toString());
}

export const writeConfig = async (config: JSON) => {
    await fs.writeFile('/etc/centreon-broker/central-broker.json', JSON.stringify(config, null, '\t'))
}