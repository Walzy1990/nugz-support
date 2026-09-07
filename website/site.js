const translations = {
  en: {
    getTool: 'Get the tool <span>↗</span>',
    heroTitle: 'Know what your machine is doing.',
    heroText: 'Nugz brings hardware inventory, platform checks, security visibility, and controlled maintenance into one focused workspace.',
    downloadNugz: 'Download Nugz <span>↓</span>',
    explore: 'Explore capabilities <span>→</span>',
    whatItSees: 'What it sees',
    capabilityTitle: 'One clear view of your Windows environment.',
    calmerWorkflow: 'A calmer workflow',
    workflowTitle: 'Inspect first. Act second.',
    workflowText: 'Nugz is built around a simple rhythm: understand the machine, save the evidence, then decide what needs attention. Online checks are explicit and limited to the action you request.',
    localScan: 'Start with a local scan <span>→</span>',
    ready: 'Ready when you are',
    downloadTitle: 'Put your system on the record.',
    downloadText: 'Download the current Windows build from this workspace. The support tool requires administrator access; the key generator is a separate utility.'
  },
  es: {
    getTool: 'Obtener la herramienta <span>↗</span>', heroTitle: 'Sabe lo que hace tu equipo.', heroText: 'Nugz reúne inventario de hardware, comprobaciones del sistema, visibilidad de seguridad y mantenimiento controlado.', downloadNugz: 'Descargar Nugz <span>↓</span>', explore: 'Explorar funciones <span>→</span>', whatItSees: 'Lo que detecta', capabilityTitle: 'Una vista clara de tu entorno Windows.', calmerWorkflow: 'Un flujo más sencillo', workflowTitle: 'Inspecciona primero. Actúa después.', workflowText: 'Nugz te ayuda a entender el equipo, guardar la información y decidir qué necesita atención. Las comprobaciones online son explícitas y limitadas a la acción solicitada.', localScan: 'Iniciar un análisis local <span>→</span>', ready: 'Cuando estés listo', downloadTitle: 'Deja constancia de tu sistema.', downloadText: 'Descarga la versión actual para Windows. La herramienta requiere permisos de administrador; el generador de claves es independiente.'
  },
  de: {
    getTool: 'Tool laden <span>↗</span>', heroTitle: 'Wissen, was dein Rechner macht.', heroText: 'Nugz vereint Hardwareinventar, Systemprüfungen, Sicherheitsstatus und kontrollierte Wartung.', downloadNugz: 'Nugz herunterladen <span>↓</span>', explore: 'Funktionen ansehen <span>→</span>', whatItSees: 'Was erkannt wird', capabilityTitle: 'Eine klare Sicht auf deine Windows-Umgebung.', calmerWorkflow: 'Ein ruhigerer Ablauf', workflowTitle: 'Erst prüfen. Dann handeln.', workflowText: 'Nugz hilft dir, den Rechner zu verstehen, Ergebnisse zu speichern und anschließend gezielt zu handeln. Online-Prüfungen werden nur auf ausdrückliche Anfrage ausgeführt.', localScan: 'Lokalen Scan starten <span>→</span>', ready: 'Bereit, wenn du es bist', downloadTitle: 'Dein System dokumentieren.', downloadText: 'Lade die aktuelle Windows-Version herunter. Das Tool benötigt Administratorrechte; der Schlüsselgenerator ist separat.'
  },
  fr: {
    getTool: 'Obtenir l’outil <span>↗</span>', heroTitle: 'Comprenez ce que fait votre machine.', heroText: 'Nugz réunit inventaire matériel, contrôles système, visibilité de sécurité et maintenance maîtrisée.', downloadNugz: 'Télécharger Nugz <span>↓</span>', explore: 'Découvrir les fonctions <span>→</span>', whatItSees: 'Ce qu’il détecte', capabilityTitle: 'Une vue claire de votre environnement Windows.', calmerWorkflow: 'Un flux plus serein', workflowTitle: 'Inspecter d’abord. Agir ensuite.', workflowText: 'Nugz aide à comprendre la machine, conserver les résultats puis décider quoi corriger. Les contrôles en ligne sont explicites et limités à votre demande.', localScan: 'Lancer un scan local <span>→</span>', ready: 'Prêt quand vous l’êtes', downloadTitle: 'Documentez votre système.', downloadText: 'Téléchargez la version Windows actuelle. L’outil nécessite les droits administrateur ; le générateur de clés est séparé.'
  },
  ja: {
    getTool: 'ツールを入手 <span>↗</span>', heroTitle: 'PCの状態を、正しく把握する。', heroText: 'Nugzはハードウェア情報、システム診断、セキュリティ状況、管理されたメンテナンスを一つにまとめます。', downloadNugz: 'Nugzをダウンロード <span>↓</span>', explore: '機能を見る <span>→</span>', whatItSees: '確認できる情報', capabilityTitle: 'Windows環境をわかりやすく表示。', calmerWorkflow: '落ち着いたワークフロー', workflowTitle: 'まず確認。次に対応。', workflowText: 'NugzはPCを理解し、結果を保存し、必要な対応を判断するためのツールです。オンライン確認は明示的な操作のときだけ実行されます。', localScan: 'ローカルスキャンを開始 <span>→</span>', ready: '準備ができたら', downloadTitle: 'システムを記録する。', downloadText: 'Windows版の最新ビルドをダウンロードできます。サポートツールには管理者権限が必要です。キー生成ツールは別アプリです。'
  }
};

const language = document.querySelector('#language');
const preferredLanguage = localStorage.getItem('nugz-language') || 'en';

function applyLanguage(locale) {
  const strings = translations[locale] || translations.en;
  document.documentElement.lang = locale;
  document.querySelectorAll('[data-i18n]').forEach((element) => {
    const value = strings[element.dataset.i18n];
    if (value) element.innerHTML = value;
  });
  localStorage.setItem('nugz-language', locale);
  language.value = locale;
}

language.addEventListener('change', () => applyLanguage(language.value));
applyLanguage(preferredLanguage);
