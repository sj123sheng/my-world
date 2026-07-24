import { execFile } from 'node:child_process';
import { createHash } from 'node:crypto';
import {
  mkdir,
  mkdtemp,
  readFile,
  rm,
  writeFile,
} from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { promisify } from 'node:util';

const execFileAsync = promisify(execFile);
const GLB_MAGIC = 0x46546c67;
const GLB_VERSION = 2;
const JSON_CHUNK = 0x4e4f534a;
const BIN_CHUNK = 0x004e4942;
const IDENTITY = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1];
const REGIONS = ['outerRing', 'centerRift', 'backdrop', 'decoration'];
const COMPONENTS = {
  5120: { bytes: 1, read: 'getInt8', write: 'setInt8' },
  5121: { bytes: 1, read: 'getUint8', write: 'setUint8' },
  5122: { bytes: 2, read: 'getInt16', write: 'setInt16' },
  5123: { bytes: 2, read: 'getUint16', write: 'setUint16' },
  5125: { bytes: 4, read: 'getUint32', write: 'setUint32' },
  5126: { bytes: 4, read: 'getFloat32', write: 'setFloat32' },
};
const TYPE_COMPONENTS = {
  SCALAR: 1,
  VEC2: 2,
  VEC3: 3,
  VEC4: 4,
  MAT2: 4,
  MAT3: 9,
  MAT4: 16,
};

export async function fetchPolyHavenFileIndex(assetId) {
  const response = await fetch(`https://api.polyhaven.com/files/${assetId}`);
  if (!response.ok) throw new Error(`${assetId}: metadata HTTP ${response.status}`);
  return response.json();
}

export function chooseGltfDownload(index, resolution = '2k') {
  const candidates = [];
  const visit = (node) => {
    if (typeof node === 'string') {
      if (node.startsWith('https://dl.polyhaven.org/') &&
          /gltf|glb/i.test(node) && node.includes(resolution)) {
        candidates.push(node);
      }
      return;
    }
    if (!node || typeof node !== 'object') return;
    if (typeof node.url === 'string' && node.url.startsWith('https://dl.polyhaven.org/') &&
        /gltf|glb/i.test(node.url) && node.url.includes(resolution)) {
      candidates.push(node.url);
    }
    for (const value of Object.values(node)) visit(value);
  };
  visit(index);
  candidates.sort();
  if (candidates.length === 0) {
    throw new Error(`no ${resolution} glTF download in Poly Haven metadata`);
  }
  return candidates[0];
}

export function sha256(bytes) {
  return createHash('sha256').update(bytes).digest('hex');
}

function asBuffer(bytes) {
  if (Buffer.isBuffer(bytes)) return bytes;
  return Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

function sortedJson(value) {
  if (Array.isArray(value)) return value.map(sortedJson);
  if (!value || typeof value !== 'object') return value;
  return Object.fromEntries(Object.keys(value).sort().map((key) => [key, sortedJson(value[key])]));
}

function cloneDocument(document) {
  return {
    json: structuredClone(document.json),
    bin: Buffer.from(asBuffer(document.bin)),
    ...(document.region ? { region: document.region } : {}),
  };
}

export function readGlb(bytes) {
  const buffer = asBuffer(bytes);
  if (buffer.length < 28 || buffer.readUInt32LE(0) !== GLB_MAGIC) {
    throw new Error('invalid GLB magic');
  }
  if (buffer.readUInt32LE(4) !== GLB_VERSION) throw new Error('GLB version must be 2');
  if (buffer.readUInt32LE(8) !== buffer.length) throw new Error('GLB total length mismatch');

  const chunks = [];
  let offset = 12;
  while (offset < buffer.length) {
    if (offset + 8 > buffer.length) throw new Error('truncated GLB chunk header');
    const length = buffer.readUInt32LE(offset);
    const type = buffer.readUInt32LE(offset + 4);
    offset += 8;
    if (offset + length > buffer.length) throw new Error('truncated GLB chunk');
    chunks.push({ type, bytes: buffer.subarray(offset, offset + length) });
    offset += length;
  }
  if (offset !== buffer.length || chunks.length !== 2 ||
      chunks[0].type !== JSON_CHUNK || chunks[1].type !== BIN_CHUNK) {
    throw new Error('GLB must contain exactly one JSON chunk followed by one BIN chunk');
  }
  let json;
  try {
    json = JSON.parse(chunks[0].bytes.toString('utf8').replace(/[\u0000 ]+$/u, ''));
  } catch (error) {
    throw new Error(`invalid GLB JSON: ${error.message}`);
  }
  return { json, bin: Buffer.from(chunks[1].bytes) };
}

export function writeGlb(document) {
  if (!document?.json || document.bin === undefined) throw new Error('document needs json and bin');
  const json = structuredClone(document.json);
  const bin = Buffer.from(asBuffer(document.bin));
  json.asset = { ...(json.asset ?? {}), version: '2.0' };
  json.buffers = [{ byteLength: bin.length }];
  const jsonBytes = Buffer.from(JSON.stringify(sortedJson(json)), 'utf8');
  const jsonPadding = (4 - (jsonBytes.length % 4)) % 4;
  const binPadding = (4 - (bin.length % 4)) % 4;
  const totalLength = 12 + 8 + jsonBytes.length + jsonPadding + 8 + bin.length + binPadding;
  const output = Buffer.alloc(totalLength);
  output.writeUInt32LE(GLB_MAGIC, 0);
  output.writeUInt32LE(GLB_VERSION, 4);
  output.writeUInt32LE(totalLength, 8);
  output.writeUInt32LE(jsonBytes.length + jsonPadding, 12);
  output.writeUInt32LE(JSON_CHUNK, 16);
  jsonBytes.copy(output, 20);
  output.fill(0x20, 20 + jsonBytes.length, 20 + jsonBytes.length + jsonPadding);
  const binHeader = 20 + jsonBytes.length + jsonPadding;
  output.writeUInt32LE(bin.length + binPadding, binHeader);
  output.writeUInt32LE(BIN_CHUNK, binHeader + 4);
  bin.copy(output, binHeader + 8);
  return output;
}

function validateVector(value, length, label) {
  if (!Array.isArray(value) || value.length !== length ||
      value.some((number) => !Number.isFinite(number))) {
    throw new Error(`${label} must contain ${length} finite numbers`);
  }
}

export function partitionNodes(document, layout) {
  const placements = Array.isArray(layout) ? layout : layout?.placements;
  if (!Array.isArray(placements)) throw new Error('layout must be an array of placements');
  const nodesByName = new Map((document.json.nodes ?? []).map((node) => [node.name, node]));
  const ids = new Set();
  for (const placement of placements) {
    if (!REGIONS.includes(placement.region)) {
      throw new Error(`${placement.id ?? '<unnamed>'}: unknown region ${placement.region}`);
    }
    if (typeof placement.id !== 'string' || placement.id.length === 0) {
      throw new Error('placement ID must be a non-empty string');
    }
    if (ids.has(placement.id)) throw new Error(`duplicate placement ID: ${placement.id}`);
    ids.add(placement.id);
    if (!nodesByName.has(placement.sourceNode)) {
      throw new Error(`${placement.id}: source node does not exist: ${placement.sourceNode}`);
    }
    validateVector(placement.translation, 3, `${placement.id}.translation`);
    validateVector(placement.rotation, 4, `${placement.id}.rotation`);
    validateVector(placement.scale, 3, `${placement.id}.scale`);
  }

  const result = {};
  for (const region of REGIONS) {
    const regionDocument = cloneDocument(document);
    const regionPlacements = placements.filter((entry) => entry.region === region)
      .sort((left, right) => left.id.localeCompare(right.id));
    regionDocument.region = region;
    regionDocument.json.nodes = regionPlacements.map((placement) => {
      const source = nodesByName.get(placement.sourceNode);
      return {
        mesh: source.mesh,
        name: placement.id,
        rotation: [...placement.rotation],
        scale: [...placement.scale],
        translation: [...placement.translation],
      };
    });
    regionDocument.json.scene = 0;
    regionDocument.json.scenes = [{
      name: region,
      nodes: regionDocument.json.nodes.map((_, index) => index),
    }];
    result[region] = regionDocument;
  }
  return result;
}

function nodeMatrix(node) {
  if (node.matrix) {
    validateVector(node.matrix, 16, `${node.name ?? 'node'}.matrix`);
    return [...node.matrix];
  }
  const translation = node.translation ?? [0, 0, 0];
  const rotation = node.rotation ?? [0, 0, 0, 1];
  const scale = node.scale ?? [1, 1, 1];
  validateVector(translation, 3, `${node.name ?? 'node'}.translation`);
  validateVector(rotation, 4, `${node.name ?? 'node'}.rotation`);
  validateVector(scale, 3, `${node.name ?? 'node'}.scale`);
  const [x, y, z, w] = rotation;
  const [sx, sy, sz] = scale;
  const x2 = x + x;
  const y2 = y + y;
  const z2 = z + z;
  const xx = x * x2;
  const xy = x * y2;
  const xz = x * z2;
  const yy = y * y2;
  const yz = y * z2;
  const zz = z * z2;
  const wx = w * x2;
  const wy = w * y2;
  const wz = w * z2;
  return [
    (1 - (yy + zz)) * sx, (xy + wz) * sx, (xz - wy) * sx, 0,
    (xy - wz) * sy, (1 - (xx + zz)) * sy, (yz + wx) * sy, 0,
    (xz + wy) * sz, (yz - wx) * sz, (1 - (xx + yy)) * sz, 0,
    translation[0], translation[1], translation[2], 1,
  ];
}

function multiplyMatrices(left, right) {
  const output = new Array(16).fill(0);
  for (let column = 0; column < 4; column += 1) {
    for (let row = 0; row < 4; row += 1) {
      for (let index = 0; index < 4; index += 1) {
        output[column * 4 + row] += left[index * 4 + row] * right[column * 4 + index];
      }
    }
  }
  return output;
}

function transformPosition(matrix, values, offset) {
  const x = values[offset];
  const y = values[offset + 1];
  const z = values[offset + 2];
  values[offset] = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12];
  values[offset + 1] = matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13];
  values[offset + 2] = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14];
}

function normalMatrix(matrix) {
  const a00 = matrix[0]; const a01 = matrix[4]; const a02 = matrix[8];
  const a10 = matrix[1]; const a11 = matrix[5]; const a12 = matrix[9];
  const a20 = matrix[2]; const a21 = matrix[6]; const a22 = matrix[10];
  const b01 = a22 * a11 - a12 * a21;
  const b11 = -a22 * a10 + a12 * a20;
  const b21 = a21 * a10 - a11 * a20;
  const determinant = a00 * b01 + a01 * b11 + a02 * b21;
  if (Math.abs(determinant) < 1e-12) throw new Error('node transform has a singular normal matrix');
  const inverse = [
    b01 / determinant,
    (-a22 * a01 + a02 * a21) / determinant,
    (a12 * a01 - a02 * a11) / determinant,
    b11 / determinant,
    (a22 * a00 - a02 * a20) / determinant,
    (-a12 * a00 + a02 * a10) / determinant,
    b21 / determinant,
    (-a21 * a00 + a01 * a20) / determinant,
    (a11 * a00 - a01 * a10) / determinant,
  ];
  return [
    inverse[0], inverse[3], inverse[6],
    inverse[1], inverse[4], inverse[7],
    inverse[2], inverse[5], inverse[8],
  ];
}

function transformNormal(matrix, values, offset) {
  const x = values[offset];
  const y = values[offset + 1];
  const z = values[offset + 2];
  const nx = matrix[0] * x + matrix[3] * y + matrix[6] * z;
  const ny = matrix[1] * x + matrix[4] * y + matrix[7] * z;
  const nz = matrix[2] * x + matrix[5] * y + matrix[8] * z;
  const length = Math.hypot(nx, ny, nz);
  if (length === 0) throw new Error('normal cannot be normalized');
  values[offset] = nx / length;
  values[offset + 1] = ny / length;
  values[offset + 2] = nz / length;
}

function decodeAccessor(document, accessorIndex) {
  const accessor = document.json.accessors?.[accessorIndex];
  if (!accessor) throw new Error(`accessor does not exist: ${accessorIndex}`);
  if (accessor.sparse) throw new Error('sparse accessors are not supported');
  const component = COMPONENTS[accessor.componentType];
  const width = TYPE_COMPONENTS[accessor.type];
  if (!component || !width) throw new Error(`unsupported accessor format: ${accessor.componentType}/${accessor.type}`);
  const values = new Array(accessor.count * width).fill(0);
  if (accessor.bufferView === undefined) return { accessor: structuredClone(accessor), values, width };
  const view = document.json.bufferViews?.[accessor.bufferView];
  if (!view || (view.buffer ?? 0) !== 0) throw new Error('accessor must use embedded buffer 0');
  const stride = view.byteStride ?? component.bytes * width;
  const start = (view.byteOffset ?? 0) + (accessor.byteOffset ?? 0);
  const bytes = asBuffer(document.bin);
  const data = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  for (let element = 0; element < accessor.count; element += 1) {
    for (let part = 0; part < width; part += 1) {
      const byteOffset = start + element * stride + part * component.bytes;
      if (byteOffset + component.bytes > bytes.length) throw new Error('accessor exceeds BIN chunk');
      values[element * width + part] = data[component.read](byteOffset, true);
    }
  }
  return { accessor: structuredClone(accessor), values, width };
}

function encodeAccessor(parts, bufferViews, accessors, values, template, target) {
  const component = COMPONENTS[template.componentType];
  const width = TYPE_COMPONENTS[template.type];
  if (!component || !width || values.length % width !== 0) throw new Error('cannot encode accessor');
  const padding = (4 - (parts.byteLength % 4)) % 4;
  if (padding) {
    parts.buffers.push(Buffer.alloc(padding));
    parts.byteLength += padding;
  }
  const bytes = Buffer.alloc(values.length * component.bytes);
  const data = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  values.forEach((value, index) => data[component.write](index * component.bytes, value, true));
  const bufferView = bufferViews.length;
  bufferViews.push({
    buffer: 0,
    byteLength: bytes.length,
    byteOffset: parts.byteLength,
    ...(target ? { target } : {}),
  });
  parts.buffers.push(bytes);
  parts.byteLength += bytes.length;
  const accessor = {
    bufferView,
    componentType: template.componentType,
    count: values.length / width,
    type: template.type,
    ...(template.normalized ? { normalized: true } : {}),
  };
  if (template.type === 'VEC3' && template.componentType === 5126) {
    accessor.min = [Infinity, Infinity, Infinity];
    accessor.max = [-Infinity, -Infinity, -Infinity];
    for (let index = 0; index < values.length; index += 3) {
      for (let axis = 0; axis < 3; axis += 1) {
        accessor.min[axis] = Math.min(accessor.min[axis], values[index + axis]);
        accessor.max[axis] = Math.max(accessor.max[axis], values[index + axis]);
      }
    }
  }
  accessors.push(accessor);
  return accessors.length - 1;
}

function primitiveRecord(document, primitive) {
  const attributes = {};
  for (const semantic of Object.keys(primitive.attributes ?? {}).sort()) {
    attributes[semantic] = decodeAccessor(document, primitive.attributes[semantic]);
  }
  const vertexCount = attributes.POSITION?.accessor.count ??
    Object.values(attributes)[0]?.accessor.count ?? 0;
  const indices = primitive.indices === undefined
    ? {
      accessor: { componentType: 5125, count: vertexCount, type: 'SCALAR' },
      values: Array.from({ length: vertexCount }, (_, index) => index),
      width: 1,
    }
    : decodeAccessor(document, primitive.indices);
  return {
    attributes,
    indices,
    material: primitive.material,
    mode: primitive.mode ?? 4,
  };
}

function rebuildGeometry(document, nodeRecords) {
  const json = structuredClone(document.json);
  const parts = { buffers: [], byteLength: 0 };
  const bufferViews = [];
  const accessors = [];
  const meshes = [];
  const nodes = [];
  for (const nodeRecord of nodeRecords) {
    const primitives = nodeRecord.primitives.map((record) => {
      const attributes = {};
      for (const semantic of Object.keys(record.attributes).sort()) {
        const decoded = record.attributes[semantic];
        attributes[semantic] = encodeAccessor(
          parts, bufferViews, accessors, decoded.values, decoded.accessor, 34962,
        );
      }
      let indexTemplate = record.indices.accessor;
      const maxIndex = Math.max(0, ...record.indices.values);
      if (maxIndex > 65535) indexTemplate = { ...indexTemplate, componentType: 5125 };
      const indices = encodeAccessor(
        parts, bufferViews, accessors, record.indices.values, indexTemplate, 34963,
      );
      return {
        attributes,
        indices,
        ...(record.material === undefined ? {} : { material: record.material }),
        ...(record.mode === 4 ? {} : { mode: record.mode }),
      };
    });
    const mesh = meshes.length;
    meshes.push({ name: nodeRecord.name, primitives });
    nodes.push({ matrix: [...IDENTITY], mesh, name: nodeRecord.name });
  }
  json.accessors = accessors;
  json.bufferViews = bufferViews;
  json.buffers = [{ byteLength: parts.byteLength }];
  json.meshes = meshes;
  json.nodes = nodes;
  json.scene = 0;
  json.scenes = [{
    name: document.region ?? json.scenes?.[0]?.name ?? 'Scene',
    nodes: nodes.map((_, index) => index),
  }];
  document.json = json;
  document.bin = Buffer.concat(parts.buffers, parts.byteLength);
  return document;
}

export function bakeNodeTransforms(document) {
  const nodes = document.json.nodes ?? [];
  const parents = new Map();
  nodes.forEach((node, index) => {
    for (const child of node.children ?? []) parents.set(child, index);
  });
  const matrices = new Map();
  const worldMatrix = (index, active = new Set()) => {
    if (matrices.has(index)) return matrices.get(index);
    if (active.has(index)) throw new Error('node hierarchy contains a cycle');
    active.add(index);
    const local = nodeMatrix(nodes[index]);
    const parent = parents.get(index);
    const world = parent === undefined ? local : multiplyMatrices(worldMatrix(parent, active), local);
    active.delete(index);
    matrices.set(index, world);
    return world;
  };

  const records = [];
  nodes.forEach((node, nodeIndex) => {
    if (node.mesh === undefined) return;
    const mesh = document.json.meshes?.[node.mesh];
    if (!mesh) throw new Error(`${node.name ?? nodeIndex}: mesh does not exist`);
    const matrix = worldMatrix(nodeIndex);
    const normals = normalMatrix(matrix);
    const primitives = mesh.primitives.map((primitive) => {
      const record = primitiveRecord(document, primitive);
      const positions = record.attributes.POSITION;
      if (!positions || positions.width !== 3) throw new Error('primitive needs VEC3 POSITION');
      for (let offset = 0; offset < positions.values.length; offset += 3) {
        transformPosition(matrix, positions.values, offset);
      }
      const normal = record.attributes.NORMAL;
      if (normal) {
        if (normal.width !== 3) throw new Error('NORMAL accessor must be VEC3');
        for (let offset = 0; offset < normal.values.length; offset += 3) {
          transformNormal(normals, normal.values, offset);
        }
      }
      return record;
    });
    records.push({ name: node.name ?? mesh.name ?? `node_${nodeIndex}`, primitives });
  });
  return rebuildGeometry(document, records);
}

export function mergePrimitivesByMaterial(region) {
  const materialNames = (region.json.materials ?? []).map((material, index) =>
    material.name ?? `material_${String(index).padStart(4, '0')}`);
  const groups = new Map();
  for (const node of region.json.nodes ?? []) {
    if (node.mesh === undefined) continue;
    for (const primitive of region.json.meshes[node.mesh].primitives) {
      const record = primitiveRecord(region, primitive);
      const name = primitive.material === undefined ? 'default' : materialNames[primitive.material];
      if (!groups.has(name)) groups.set(name, []);
      groups.get(name).push(record);
    }
  }
  const originalMaterials = new Map((region.json.materials ?? []).map((material, index) =>
    [materialNames[index], material]));
  const sortedNames = [...groups.keys()].sort();
  region.json.materials = sortedNames.map((name) =>
    structuredClone(originalMaterials.get(name) ?? { name }));
  const merged = sortedNames.map((name, material) => {
    const records = groups.get(name);
    const semantics = Object.keys(records[0].attributes).sort();
    const attributes = {};
    for (const semantic of semantics) {
      const template = records[0].attributes[semantic].accessor;
      attributes[semantic] = {
        accessor: { ...template, count: 0 },
        values: [],
        width: records[0].attributes[semantic].width,
      };
    }
    const indices = {
      accessor: { componentType: 5125, count: 0, type: 'SCALAR' },
      values: [],
      width: 1,
    };
    let vertexBase = 0;
    for (const record of records) {
      if (record.mode !== records[0].mode) throw new Error(`${name}: primitive modes differ`);
      for (const semantic of semantics) {
        if (!record.attributes[semantic]) throw new Error(`${name}: attribute sets differ`);
        attributes[semantic].values.push(...record.attributes[semantic].values);
      }
      indices.values.push(...record.indices.values.map((index) => index + vertexBase));
      vertexBase += record.attributes[semantics[0]].accessor.count;
    }
    for (const attribute of Object.values(attributes)) {
      attribute.accessor.count = attribute.values.length / attribute.width;
    }
    indices.accessor.count = indices.values.length;
    return { attributes, indices, material, mode: records[0].mode };
  });
  return rebuildGeometry(region, [{
    name: region.region ?? 'region',
    primitives: merged,
  }]);
}

function imageMimeType(bytes) {
  const buffer = asBuffer(bytes);
  if (buffer.length >= 3 && buffer[0] === 0xff && buffer[1] === 0xd8 && buffer[2] === 0xff) {
    return 'image/jpeg';
  }
  if (buffer.length >= 4 && buffer[0] === 0x89 && buffer.subarray(1, 4).toString('ascii') === 'PNG') {
    return 'image/png';
  }
  throw new Error('diffuse texture must be PNG or JPEG');
}

export function embedTextureLevels(region, diffuse2k, diffuse1k) {
  const full = Buffer.from(asBuffer(diffuse2k));
  const half = Buffer.from(asBuffer(diffuse1k));
  const parts = [Buffer.from(asBuffer(region.bin))];
  let byteLength = parts[0].length;
  const appendImage = (bytes, name) => {
    const padding = (4 - (byteLength % 4)) % 4;
    if (padding) {
      parts.push(Buffer.alloc(padding));
      byteLength += padding;
    }
    const bufferView = region.json.bufferViews.length;
    region.json.bufferViews.push({ buffer: 0, byteLength: bytes.length, byteOffset: byteLength });
    parts.push(bytes);
    byteLength += bytes.length;
    return { bufferView, mimeType: imageMimeType(bytes), name };
  };
  region.json.images = [
    appendImage(full, 'diffuse_full'),
    appendImage(half, 'diffuse_half'),
  ];
  region.json.samplers = [{ magFilter: 9729, minFilter: 9987, wrapS: 10497, wrapT: 10497 }];
  region.json.textures = [
    { name: 'diffuse_full', sampler: 0, source: 0 },
    { name: 'diffuse_half', sampler: 0, source: 1 },
  ];
  region.json.materials = (region.json.materials ?? [{ name: 'stone' }])
    .sort((left, right) => (left.name ?? '').localeCompare(right.name ?? ''))
    .map((material) => ({
      name: material.name ?? 'stone',
      pbrMetallicRoughness: {
        baseColorTexture: { index: 0 },
        metallicFactor: 0,
        roughnessFactor: 1,
      },
      extras: {
        textureLevels: {
          full: 0,
          half: 1,
        },
      },
    }));
  region.bin = Buffer.concat(parts, byteLength);
  region.json.buffers = [{ byteLength }];
  return region;
}

function findDownloadRecord(index, url) {
  let found;
  const visit = (node) => {
    if (found || !node || typeof node !== 'object') return;
    if (node.url === url) {
      found = node;
      return;
    }
    for (const value of Object.values(node)) visit(value);
  };
  visit(index);
  if (!found) throw new Error(`download metadata missing for ${url}`);
  return found;
}

async function download(url) {
  const response = await fetch(url);
  if (!response.ok) throw new Error(`${url}: download HTTP ${response.status}`);
  return Buffer.from(await response.arrayBuffer());
}

async function downloadPackage(index, url, directory) {
  const record = findDownloadRecord(index, url);
  const primary = await download(url);
  const primaryPath = join(directory, new URL(url).pathname.split('/').at(-1));
  await mkdir(directory, { recursive: true });
  await writeFile(primaryPath, primary);
  const includes = {};
  await Promise.all(Object.entries(record.include ?? {}).map(async ([relativePath, dependency]) => {
    const bytes = await download(dependency.url);
    const path = join(directory, relativePath);
    await mkdir(dirname(path), { recursive: true });
    await writeFile(path, bytes);
    includes[relativePath] = { bytes, path, url: dependency.url };
  }));
  return { primary, primaryPath, includes, url };
}

function rejectExternalUris(document) {
  for (const buffer of document.json.buffers ?? []) {
    if (buffer.uri) throw new Error(`external buffer URI remains: ${buffer.uri}`);
  }
  for (const image of document.json.images ?? []) {
    if (image.uri) throw new Error(`external image URI remains: ${image.uri}`);
    if (image.bufferView === undefined) throw new Error(`${image.name ?? 'image'} is not embedded`);
  }
}

async function generateHalfTexture(inputPath, outputPath) {
  await execFileAsync('/usr/bin/sips', [
    '--resampleHeightWidth', '1024', '1024',
    '--setProperty', 'format', 'png',
    '--setProperty', 'formatOptions', 'normal',
    inputPath,
    '--out', outputPath,
  ]);
  return readFile(outputPath);
}

async function main() {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
  const layoutPath = join(root, 'assets/environment/layout.json');
  const outputDirectory = join(root, 'entry/src/main/resources/rawfile/environment');
  const manifestPath = join(root, 'assets/environment/manifest.json');
  const temporary = await mkdtemp(join(tmpdir(), 'my-world-environment-'));
  try {
    const [fortIndex, ruinsIndex] = await Promise.all([
      fetchPolyHavenFileIndex('modular_fort_01'),
      fetchPolyHavenFileIndex('rabdentse_ruins_wall'),
    ]);
    const fortUrl = chooseGltfDownload(fortIndex, '2k');
    const ruinsUrl = chooseGltfDownload(ruinsIndex, '2k');
    const [fort, ruins] = await Promise.all([
      downloadPackage(fortIndex, fortUrl, join(temporary, 'modular_fort_01')),
      downloadPackage(ruinsIndex, ruinsUrl, join(temporary, 'rabdentse_ruins_wall')),
    ]);
    const fortJson = JSON.parse(fort.primary.toString('utf8'));
    const fortBinEntry = fort.includes[fortJson.buffers[0].uri];
    if (!fortBinEntry) throw new Error(`fort BIN dependency missing: ${fortJson.buffers[0].uri}`);
    const sourceDocument = { json: fortJson, bin: fortBinEntry.bytes };
    const layout = JSON.parse(await readFile(layoutPath, 'utf8'));
    const regions = partitionNodes(sourceDocument, layout);
    const diffuseEntry = Object.entries(ruins.includes)
      .find(([name]) => /_diff_2k\.(?:jpg|jpeg|png)$/i.test(name))?.[1];
    if (!diffuseEntry) throw new Error('Rabdentse 2K diffuse texture dependency missing');
    const diffuseHalf = await generateHalfTexture(
      diffuseEntry.path, join(temporary, 'rabdentse_ruins_wall_diff_1k.png'),
    );
    await mkdir(outputDirectory, { recursive: true });
    const derivedAssets = [];
    const filenames = {
      outerRing: 'outer_ring.glb',
      centerRift: 'center_rift.glb',
      backdrop: 'backdrop.glb',
      decoration: 'decoration.glb',
    };
    for (const regionName of REGIONS) {
      const document = regions[regionName];
      bakeNodeTransforms(document);
      mergePrimitivesByMaterial(document);
      embedTextureLevels(document, diffuseEntry.bytes, diffuseHalf);
      rejectExternalUris(document);
      const bytes = writeGlb(document);
      const relativePath = `entry/src/main/resources/rawfile/environment/${filenames[regionName]}`;
      await writeFile(join(root, relativePath), bytes);
      derivedAssets.push({
        id: regionName,
        path: relativePath,
        sha256: sha256(bytes),
        sourceDependencies: ['modular_fort_01', 'rabdentse_ruins_wall'],
      });
    }
    const downloadedAt = new Date().toISOString().slice(0, 10);
    const manifest = {
      license: 'CC0-1.0',
      licenseUrl: 'https://polyhaven.com/license',
      sourceAssets: [
        {
          id: 'modular_fort_01',
          name: 'Modular Fort 01',
          author: 'Rico Cilliers',
          pageUrl: 'https://polyhaven.com/a/modular_fort_01',
          downloadUrl: fortUrl,
          sha256: sha256(fort.primary),
          downloadedAt,
        },
        {
          id: 'rabdentse_ruins_wall',
          name: 'Rabdentse Ruins Wall',
          author: 'Amal Kumar',
          pageUrl: 'https://polyhaven.com/a/rabdentse_ruins_wall',
          downloadUrl: ruinsUrl,
          sha256: sha256(ruins.primary),
          downloadedAt,
        },
      ],
      derivedAssets,
    };
    await mkdir(dirname(manifestPath), { recursive: true });
    await writeFile(manifestPath, `${JSON.stringify(sortedJson(manifest), null, 2)}\n`);
  } finally {
    await rm(temporary, { recursive: true, force: true });
  }
}

if (process.argv[1] &&
    pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  await main();
}
