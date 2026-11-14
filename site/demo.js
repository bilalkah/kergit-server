(() => {
  "use strict";

  const demoApp = document.querySelector("#demo-app");
  if (!demoApp) return;

  const workspaces = {
    kergit: {
      name: "Kergit Lab",
      subtitle: "Prototype workspace",
      slug: "kergit-lab",
      channels: [
        {
          title: "Text channels",
          type: "text",
          items: [
            { id: "genel", name: "genel", topic: "Kergit için ortak alan", unread: false },
            { id: "cpp-core", name: "cpp-core", topic: "Signaling, queues and protocol", unread: true },
            { id: "ui-lab", name: "ui-lab", topic: "Client polish and product experiments", unread: false },
          ],
        },
        {
          title: "Voice channels",
          type: "voice",
          items: [
            { id: "build-room", name: "Build room", participants: ["ece", "deniz"] },
            { id: "focus-room", name: "Focus room", participants: ["mert"] },
          ],
        },
      ],
      members: [
        { id: "bilal", name: "Bilal", initials: "BK", status: "online", activity: "Building Kergit", role: "owner", color: ["#7c3aed", "#2563eb"] },
        { id: "ece", name: "Ece", initials: "EC", status: "online", activity: "In Build room", role: "admin", color: ["#0891b2", "#2563eb"], speaking: true },
        { id: "deniz", name: "Deniz", initials: "DZ", status: "online", activity: "Reviewing the UI", color: ["#059669", "#0d9488"] },
        { id: "mert", name: "Mert", initials: "MT", status: "online", activity: "In Focus room", color: ["#c2410c", "#db2777"] },
        { id: "selin", name: "Selin", initials: "SL", status: "online", activity: "Reading #cpp-core", color: ["#be185d", "#7c3aed"] },
        { id: "arda", name: "Arda", initials: "AR", status: "offline", activity: "Offline", color: ["#475569", "#334155"] },
        { id: "irem", name: "İrem", initials: "İR", status: "offline", activity: "Offline", color: ["#475569", "#1e293b"] },
        { id: "kaan", name: "Kaan", initials: "KN", status: "offline", activity: "Offline", color: ["#4b5563", "#374151"] },
      ],
      messages: {
        genel: [
          {
            author: "Bilal",
            initials: "BK",
            time: "20:38",
            text: "Welcome to the interactive Kergit workspace. Everything in this window stays in your browser.",
            badge: "owner",
            color: ["#7c3aed", "#2563eb"],
            reactions: ["👋 4", "⚡ 2"],
          },
          {
            author: "Ece",
            initials: "EC",
            time: "20:41",
            text: "The new voice ownership path feels much clearer. One session owns voice, the other tabs stay synchronized.",
            color: ["#0891b2", "#2563eb"],
            reactions: ["✅ 3"],
          },
          {
            author: "Deniz",
            initials: "DZ",
            time: "20:43",
            text: "I pushed a pass on the channel layout. Try switching to #cpp-core, then join Build room.",
            color: ["#059669", "#0d9488"],
          },
        ],
        "cpp-core": [
          {
            author: "Bilal",
            initials: "BK",
            time: "18:12",
            text: "The outbound path now keeps authoritative state ahead of repairable real-time signals.",
            badge: "owner",
            color: ["#7c3aed", "#2563eb"],
            codeBlock: "P0  state mutation   → never drop\nP1  message delta    → preserve order\nP2  presence update  → repair via sync\nP3  typing signal    → drop under load",
            reactions: ["🧠 5", "⚙️ 3"],
          },
          {
            author: "Selin",
            initials: "SL",
            time: "18:18",
            text: "That makes reconnect behavior much easier to reason about. The snapshot remains the repair boundary.",
            color: ["#be185d", "#7c3aed"],
          },
          {
            author: "Mert",
            initials: "MT",
            time: "18:24",
            text: "And the protobuf envelope stays stable while each payload evolves independently.",
            color: ["#c2410c", "#db2777"],
            reactions: ["👍 2"],
          },
        ],
        "ui-lab": [
          {
            author: "Deniz",
            initials: "DZ",
            time: "16:04",
            text: "The compact member panel is working. On small screens it becomes a slide-over instead of shrinking chat.",
            color: ["#059669", "#0d9488"],
          },
          {
            author: "Ece",
            initials: "EC",
            time: "16:09",
            text: "Nice. I also like that simulated controls never trigger a browser permission prompt.",
            color: ["#0891b2", "#2563eb"],
            reactions: ["✨ 4"],
          },
        ],
      },
    },
    opensource: {
      name: "Open Source Türkiye",
      subtitle: "Community workspace",
      slug: "open-source-tr",
      channels: [
        {
          title: "Text channels",
          type: "text",
          items: [
            { id: "duyurular", name: "duyurular", topic: "Community announcements", unread: true },
            { id: "projeler", name: "projeler", topic: "What everyone is building", unread: false },
            { id: "yardim", name: "yardım", topic: "Ask, learn, and unblock", unread: false },
          ],
        },
        {
          title: "Voice channels",
          type: "voice",
          items: [
            { id: "community-stage", name: "Community stage", participants: ["aylin"] },
            { id: "coffee-room", name: "Coffee room", participants: [] },
          ],
        },
      ],
      members: [
        { id: "bilal", name: "Bilal", initials: "BK", status: "online", activity: "Showing Kergit", color: ["#7c3aed", "#2563eb"] },
        { id: "aylin", name: "Aylin", initials: "AY", status: "online", activity: "On Community stage", role: "host", color: ["#0d9488", "#2563eb"], speaking: true },
        { id: "emre", name: "Emre", initials: "EM", status: "online", activity: "Browsing projects", color: ["#ea580c", "#ca8a04"] },
        { id: "zeynep", name: "Zeynep", initials: "ZY", status: "online", activity: "Online", color: ["#db2777", "#7c3aed"] },
        { id: "can", name: "Can", initials: "CN", status: "offline", activity: "Offline", color: ["#475569", "#334155"] },
        { id: "eda", name: "Eda", initials: "ED", status: "offline", activity: "Offline", color: ["#4b5563", "#1e293b"] },
      ],
      messages: {
        duyurular: [
          {
            author: "Aylin",
            initials: "AY",
            time: "09:30",
            text: "Demo workspace notice: these conversations are illustrative and are not real community messages.",
            badge: "host",
            color: ["#0d9488", "#2563eb"],
          },
          {
            author: "Bilal",
            initials: "BK",
            time: "09:42",
            text: "Kergit’s project page now lets visitors experience the interface before diving into the architecture.",
            color: ["#7c3aed", "#2563eb"],
            reactions: ["🚀 6"],
          },
        ],
        projeler: [
          {
            author: "Emre",
            initials: "EM",
            time: "14:10",
            text: "What makes this project interesting is the full path: browser UI, protobuf transport, C++ authority, and media orchestration.",
            color: ["#ea580c", "#ca8a04"],
          },
          {
            author: "Zeynep",
            initials: "ZY",
            time: "14:14",
            text: "The honest demo boundary helps too—interactive enough to understand, without pretending a public backend exists.",
            color: ["#db2777", "#7c3aed"],
            reactions: ["💜 3"],
          },
        ],
        yardim: [
          {
            author: "Bilal",
            initials: "BK",
            time: "12:07",
            text: "Start with docs/ARCHITECTURE.md, then follow VOICE_JOIN through the service and LiveKit adapter.",
            color: ["#7c3aed", "#2563eb"],
            codeBlock: "browser → Caddy → C++ core → LiveKit\n                     ↘ PostgreSQL / Redis",
          },
        ],
      },
    },
    nightshift: {
      name: "Night Shift",
      subtitle: "Friends and games",
      slug: "night-shift",
      channels: [
        {
          title: "Text channels",
          type: "text",
          items: [
            { id: "lobby", name: "lobby", topic: "Tonight’s plans", unread: false },
            { id: "clips", name: "clips", topic: "Screenshots and good moments", unread: true },
          ],
        },
        {
          title: "Voice channels",
          type: "voice",
          items: [
            { id: "squad-one", name: "Squad one", participants: ["mira", "baran", "ozan"] },
            { id: "afk", name: "AFK", participants: [] },
          ],
        },
      ],
      members: [
        { id: "bilal", name: "Bilal", initials: "BK", status: "online", activity: "In Kergit Lab", color: ["#7c3aed", "#2563eb"] },
        { id: "mira", name: "Mira", initials: "MR", status: "online", activity: "In Squad one", color: ["#db2777", "#7c3aed"], speaking: true },
        { id: "baran", name: "Baran", initials: "BR", status: "online", activity: "In Squad one", color: ["#0284c7", "#0891b2"] },
        { id: "ozan", name: "Ozan", initials: "OZ", status: "online", activity: "In Squad one", color: ["#65a30d", "#059669"] },
        { id: "lale", name: "Lale", initials: "LL", status: "offline", activity: "Offline", color: ["#475569", "#334155"] },
      ],
      messages: {
        lobby: [
          {
            author: "Mira",
            initials: "MR",
            time: "22:03",
            text: "Squad one is open. The green speaking ring is simulated, but this is how the real client presents voice activity.",
            color: ["#db2777", "#7c3aed"],
            reactions: ["🎮 3"],
          },
          {
            author: "Baran",
            initials: "BR",
            time: "22:05",
            text: "Joining now. No microphone prompt in this demo—exactly as promised.",
            color: ["#0284c7", "#0891b2"],
          },
        ],
        clips: [
          {
            author: "Ozan",
            initials: "OZ",
            time: "23:18",
            text: "Attachments are intentionally left out of the static simulation, but the real client has an attachment tray and previews.",
            color: ["#65a30d", "#059669"],
          },
        ],
      },
    },
  };

  const initialMessages = JSON.parse(JSON.stringify(
    Object.fromEntries(Object.entries(workspaces).map(([key, workspace]) => [key, workspace.messages]))
  ));

  const state = {
    hub: "kergit",
    channelByHub: {
      kergit: "genel",
      opensource: "duyurular",
      nightshift: "lobby",
    },
    messages: JSON.parse(JSON.stringify(initialMessages)),
    voice: null,
    muted: false,
    deafened: false,
    search: "",
    botTimer: null,
    toastTimer: null,
  };

  const elements = {
    workspaceName: document.querySelector("#workspace-name"),
    channelList: document.querySelector("#channel-list"),
    channelName: document.querySelector("#channel-name"),
    channelTopic: document.querySelector("#channel-topic"),
    messageList: document.querySelector("#message-list"),
    memberList: document.querySelector("#member-list"),
    memberTotal: document.querySelector("#member-total"),
    messageForm: document.querySelector("#message-form"),
    messageInput: document.querySelector("#message-input"),
    search: document.querySelector("#demo-search"),
    voiceDock: document.querySelector("#voice-dock"),
    voiceDockChannel: document.querySelector("#voice-dock-channel"),
    mute: document.querySelector("#demo-mute"),
    deafen: document.querySelector("#demo-deafen"),
    leaveVoice: document.querySelector("#voice-leave"),
    camera: document.querySelector("#demo-camera"),
    screen: document.querySelector("#demo-screen"),
    reset: document.querySelector("#demo-reset"),
    toast: document.querySelector("#demo-toast"),
    typing: document.querySelector("#typing-row"),
    popover: document.querySelector("#demo-popover"),
    scrim: document.querySelector("#demo-scrim"),
    channelsToggle: document.querySelector("#channels-toggle"),
    membersToggle: document.querySelector("#members-toggle"),
    address: document.querySelector(".browser-address span:last-child"),
  };

  function currentWorkspace() {
    return workspaces[state.hub];
  }

  function currentChannelId() {
    return state.channelByHub[state.hub];
  }

  function currentChannel() {
    const id = currentChannelId();
    return currentWorkspace().channels
      .flatMap((section) => section.items)
      .find((channel) => channel.id === id);
  }

  function currentMessages() {
    return state.messages[state.hub][currentChannelId()] || [];
  }

  function avatarStyle(colors) {
    const [start, end] = colors || ["#475569", "#1e293b"];
    return `linear-gradient(145deg, ${start}, ${end})`;
  }

  function createAvatar(initials, colors, className = "") {
    const avatar = document.createElement("span");
    avatar.className = `avatar ${className}`.trim();
    avatar.textContent = initials;
    avatar.style.background = avatarStyle(colors);
    return avatar;
  }

  function showToast(message) {
    window.clearTimeout(state.toastTimer);
    elements.toast.textContent = message;
    elements.toast.classList.add("visible");
    state.toastTimer = window.setTimeout(() => {
      elements.toast.classList.remove("visible");
    }, 2700);
  }

  function closePanels() {
    demoApp.classList.remove("channels-open", "members-open");
    elements.channelsToggle.setAttribute("aria-expanded", "false");
    if (window.matchMedia("(max-width: 1120px)").matches) {
      elements.membersToggle.setAttribute("aria-expanded", "false");
    }
    elements.popover.hidden = true;
  }

  function showPopover(anchor, title, copy, actionLabel, onAction) {
    elements.popover.replaceChildren();

    const heading = document.createElement("h4");
    heading.textContent = title;
    const description = document.createElement("p");
    description.textContent = copy;
    const action = document.createElement("button");
    action.type = "button";
    action.textContent = actionLabel;
    action.addEventListener("click", () => {
      onAction();
      elements.popover.hidden = true;
    }, { once: true });

    elements.popover.append(heading, description, action);
    elements.popover.hidden = false;

    const anchorRect = anchor.getBoundingClientRect();
    const appRect = demoApp.getBoundingClientRect();
    const desiredLeft = anchorRect.left - appRect.left + anchorRect.width + 10;
    const desiredTop = anchorRect.top - appRect.top;
    const maxLeft = Math.max(12, appRect.width - 294);
    const maxTop = Math.max(12, appRect.height - 150);
    elements.popover.style.left = `${Math.min(Math.max(12, desiredLeft), maxLeft)}px`;
    elements.popover.style.top = `${Math.min(Math.max(12, desiredTop), maxTop)}px`;
  }

  function renderChannels() {
    const workspace = currentWorkspace();
    elements.channelList.replaceChildren();

    workspace.channels.forEach((section) => {
      const wrapper = document.createElement("section");
      wrapper.className = "channel-section";

      const heading = document.createElement("div");
      heading.className = "channel-section-head";
      const title = document.createElement("span");
      title.textContent = section.title;
      const add = document.createElement("button");
      add.type = "button";
      add.textContent = "+";
      add.setAttribute("aria-label", `Add to ${section.title}`);
      add.addEventListener("click", () => {
        showPopover(
          add,
          section.type === "voice" ? "Create voice channel" : "Create text channel",
          "This static demo keeps the creation flow local and temporary.",
          "Create a demo channel",
          () => showToast("Demo channel created for this preview — nothing was saved.")
        );
      });
      heading.append(title, add);
      wrapper.append(heading);

      section.items.forEach((channel) => {
        const button = document.createElement("button");
        button.type = "button";
        button.className = "channel-button";
        button.dataset.channel = channel.id;
        button.dataset.type = section.type;

        const isTextActive = section.type === "text" && channel.id === currentChannelId();
        const isVoiceActive = section.type === "voice" && state.voice
          && state.voice.hub === state.hub
          && state.voice.channel === channel.id;
        button.classList.toggle("active", Boolean(isTextActive || isVoiceActive));

        const icon = document.createElement("span");
        icon.className = "channel-icon";
        icon.textContent = section.type === "voice" ? "⌁" : "#";
        const name = document.createElement("span");
        name.textContent = channel.name;
        button.append(icon, name);

        if (channel.unread && !isTextActive) {
          const unread = document.createElement("i");
          unread.className = "channel-unread";
          unread.setAttribute("aria-label", "Unread");
          button.append(unread);
        }

        button.addEventListener("click", () => {
          if (section.type === "voice") {
            joinVoice(channel);
            return;
          }
          state.channelByHub[state.hub] = channel.id;
          state.search = "";
          elements.search.value = "";
          renderAll();
          if (window.matchMedia("(max-width: 760px)").matches) closePanels();
        });
        wrapper.append(button);

        if (section.type === "voice") {
          const participantIds = [...channel.participants];
          if (isVoiceActive && !participantIds.includes("bilal")) participantIds.push("bilal");
          if (participantIds.length) {
            const users = document.createElement("div");
            users.className = "voice-users";
            participantIds.forEach((participantId) => {
              const member = workspace.members.find((item) => item.id === participantId)
                || workspaces[state.voice?.hub]?.members.find((item) => item.id === participantId);
              if (!member) return;
              const row = document.createElement("div");
              row.className = `voice-user ${member.speaking ? "speaking" : ""}`.trim();
              const avatar = document.createElement("span");
              avatar.className = "mini-avatar";
              avatar.textContent = member.initials;
              avatar.style.background = avatarStyle(member.color);
              const memberName = document.createElement("span");
              memberName.textContent = member.id === "bilal" ? "You" : member.name;
              row.append(avatar, memberName);
              users.append(row);
            });
            wrapper.append(users);
          }
        }
      });

      elements.channelList.append(wrapper);
    });
  }

  function renderMessage(message) {
    const row = document.createElement("article");
    row.className = `message ${message.local ? "is-local" : ""}`.trim();
    row.dataset.searchText = `${message.author} ${message.text} ${message.codeBlock || ""}`.toLowerCase();

    const avatar = createAvatar(message.initials, message.color);
    const copy = document.createElement("div");
    copy.className = "message-copy";

    const meta = document.createElement("div");
    meta.className = "message-meta";
    const author = document.createElement("strong");
    author.textContent = message.author;
    meta.append(author);
    if (message.badge) {
      const badge = document.createElement("span");
      badge.className = "message-badge";
      badge.textContent = message.badge;
      meta.append(badge);
    }
    const time = document.createElement("time");
    time.textContent = message.time;
    meta.append(time);

    const body = document.createElement("p");
    body.textContent = message.text;
    copy.append(meta, body);

    if (message.codeBlock) {
      const code = document.createElement("pre");
      code.className = "message-codeblock";
      code.textContent = message.codeBlock;
      copy.append(code);
    }

    if (message.reactions?.length) {
      const reactions = document.createElement("div");
      reactions.className = "message-reactions";
      message.reactions.forEach((reaction) => {
        const chip = document.createElement("span");
        chip.className = "message-reaction";
        chip.textContent = reaction;
        reactions.append(chip);
      });
      copy.append(reactions);
    }

    row.append(avatar, copy);
    return row;
  }

  function renderMessages() {
    const channel = currentChannel();
    const messages = currentMessages();
    elements.messageList.replaceChildren();

    const welcome = document.createElement("div");
    welcome.className = "channel-welcome";
    const symbol = document.createElement("span");
    symbol.className = "welcome-symbol";
    symbol.textContent = "#";
    const title = document.createElement("h3");
    title.textContent = `Welcome to #${channel.name}`;
    const description = document.createElement("p");
    description.textContent = channel.topic;
    welcome.append(symbol, title, description);
    elements.messageList.append(welcome);

    const divider = document.createElement("div");
    divider.className = "date-divider";
    divider.textContent = "Today";
    elements.messageList.append(divider);

    const query = state.search.trim().toLowerCase();
    let shown = 0;
    messages.forEach((message) => {
      const row = renderMessage(message);
      if (query && !row.dataset.searchText.includes(query)) return;
      elements.messageList.append(row);
      shown += 1;
    });

    if (!shown) {
      const empty = document.createElement("div");
      empty.className = "channel-welcome";
      const emptyTitle = document.createElement("h3");
      emptyTitle.textContent = "No matching messages";
      const emptyCopy = document.createElement("p");
      emptyCopy.textContent = "Try another phrase. Search is local to this demo channel.";
      empty.append(emptyTitle, emptyCopy);
      elements.messageList.append(empty);
    }

    window.requestAnimationFrame(() => {
      elements.messageList.scrollTop = elements.messageList.scrollHeight;
    });
  }

  function renderMembers() {
    const members = currentWorkspace().members;
    elements.memberList.replaceChildren();
    elements.memberTotal.textContent = String(members.length);

    const groups = [
      ["Online", members.filter((member) => member.status === "online")],
      ["Offline", members.filter((member) => member.status === "offline")],
    ];

    groups.forEach(([label, groupMembers]) => {
      const group = document.createElement("section");
      group.className = "member-group";
      const heading = document.createElement("h3");
      heading.className = "member-group-title";
      heading.textContent = `${label} — ${groupMembers.length}`;
      group.append(heading);

      groupMembers.forEach((member) => {
        const button = document.createElement("button");
        button.type = "button";
        button.className = `member-item ${member.status === "offline" ? "offline" : ""} ${member.speaking ? "speaking" : ""}`.trim();
        const avatar = createAvatar(member.initials, member.color);
        const copy = document.createElement("span");
        copy.className = "member-item-copy";
        const name = document.createElement("strong");
        name.textContent = member.id === "bilal" ? `${member.name} (you)` : member.name;
        const activity = document.createElement("span");
        activity.textContent = member.activity;
        copy.append(name, activity);
        button.append(avatar, copy);

        if (member.role) {
          const role = document.createElement("span");
          role.className = "member-role";
          role.textContent = member.role;
          button.append(role);
        }

        button.addEventListener("click", () => {
          showPopover(
            button,
            member.name,
            `${member.activity}. This profile is illustrative demo data.`,
            "Mention in composer",
            () => {
              elements.messageInput.value = `@${member.name} `;
              elements.messageInput.focus();
            }
          );
        });
        group.append(button);
      });

      elements.memberList.append(group);
    });
  }

  function renderVoiceDock() {
    if (!state.voice) {
      elements.voiceDock.hidden = true;
      return;
    }
    elements.voiceDock.hidden = false;
    elements.voiceDockChannel.textContent = state.voice.name;
  }

  function updateControl(button, active, activeIcon, inactiveIcon, activeLabel, inactiveLabel) {
    button.setAttribute("aria-pressed", String(active));
    button.setAttribute("aria-label", active ? activeLabel : inactiveLabel);
    const use = button.querySelector("use");
    if (use) use.setAttribute("href", active ? activeIcon : inactiveIcon);
  }

  function renderControls() {
    updateControl(elements.mute, state.muted, "#icon-mic-off", "#icon-mic", "Unmute microphone", "Mute microphone");
    elements.deafen.setAttribute("aria-pressed", String(state.deafened));
    elements.deafen.setAttribute("aria-label", state.deafened ? "Undeafen audio" : "Deafen audio");
  }

  function renderHeader() {
    const workspace = currentWorkspace();
    const channel = currentChannel();
    elements.workspaceName.textContent = workspace.name;
    elements.channelName.textContent = channel.name;
    elements.channelTopic.textContent = channel.topic;
    elements.messageInput.placeholder = `Message #${channel.name}`;
    elements.address.textContent = `kergit.local/channels/${workspace.slug}/${channel.id}`;

    document.querySelectorAll("[data-hub]").forEach((button) => {
      const active = button.dataset.hub === state.hub;
      button.classList.toggle("active", active);
      button.setAttribute("aria-pressed", String(active));
    });
  }

  function renderAll() {
    renderHeader();
    renderChannels();
    renderMessages();
    renderMembers();
    renderVoiceDock();
    renderControls();
  }

  function joinVoice(channel) {
    const alreadyConnected = state.voice
      && state.voice.hub === state.hub
      && state.voice.channel === channel.id;
    if (alreadyConnected) {
      leaveVoice();
      return;
    }

    state.voice = {
      hub: state.hub,
      channel: channel.id,
      name: channel.name,
    };
    renderChannels();
    renderVoiceDock();
    showToast(`Joined ${channel.name} locally — no microphone or WebRTC connection was opened.`);
    if (window.matchMedia("(max-width: 760px)").matches) closePanels();
  }

  function leaveVoice() {
    if (!state.voice) return;
    const channelName = state.voice.name;
    state.voice = null;
    state.muted = false;
    state.deafened = false;
    renderChannels();
    renderVoiceDock();
    renderControls();
    showToast(`Left ${channelName}. The simulated voice state was cleared.`);
  }

  function formatTime() {
    return new Intl.DateTimeFormat("en", {
      hour: "2-digit",
      minute: "2-digit",
      hour12: false,
    }).format(new Date());
  }

  function sendMessage(text) {
    const message = {
      author: "Bilal",
      initials: "BK",
      time: formatTime(),
      text,
      badge: "you",
      color: ["#7c3aed", "#2563eb"],
      local: true,
    };
    currentMessages().push(message);
    state.search = "";
    elements.search.value = "";
    renderMessages();
    showToast("Message rendered locally. Nothing was sent or stored.");

    window.clearTimeout(state.botTimer);
    elements.typing.hidden = false;
    const sentHub = state.hub;
    const sentChannel = currentChannelId();
    state.botTimer = window.setTimeout(() => {
      elements.typing.hidden = true;
      state.messages[sentHub][sentChannel].push({
        author: "Kergit bot",
        initials: "KG",
        time: formatTime(),
        text: "Local demo confirmed: your message stayed in this browser tab.",
        badge: "demo",
        color: ["#0d9488", "#0891b2"],
        local: true,
      });
      if (state.hub === sentHub && currentChannelId() === sentChannel) renderMessages();
    }, 1100);
  }

  document.querySelectorAll("[data-hub]").forEach((button) => {
    button.addEventListener("click", () => {
      state.hub = button.dataset.hub;
      state.search = "";
      elements.search.value = "";
      renderAll();
      showToast(`Switched to ${currentWorkspace().name}. Demo state is still local.`);
    });
  });

  elements.messageForm.addEventListener("submit", (event) => {
    event.preventDefault();
    const text = elements.messageInput.value.trim();
    if (!text) {
      elements.messageInput.focus();
      showToast("Type a message first — it will only appear in this local demo.");
      return;
    }
    elements.messageInput.value = "";
    sendMessage(text);
  });

  elements.search.addEventListener("input", () => {
    state.search = elements.search.value;
    renderMessages();
  });

  elements.mute.addEventListener("click", () => {
    state.muted = !state.muted;
    renderControls();
    showToast(state.muted ? "Microphone muted in the simulation." : "Microphone unmuted in the simulation.");
  });

  elements.deafen.addEventListener("click", () => {
    state.deafened = !state.deafened;
    if (state.deafened) state.muted = true;
    renderControls();
    showToast(state.deafened ? "Audio deafened locally. No real audio exists." : "Audio undeafened in the simulation.");
  });

  elements.leaveVoice.addEventListener("click", leaveVoice);

  [elements.camera, elements.screen].forEach((button) => {
    button.addEventListener("click", () => {
      const pressed = button.getAttribute("aria-pressed") === "true";
      button.setAttribute("aria-pressed", String(!pressed));
      const feature = button === elements.camera ? "Camera" : "Screen sharing";
      showToast(`${feature} preview ${pressed ? "disabled" : "enabled"} — no browser permission was requested.`);
    });
  });

  document.querySelector("#demo-add-hub").addEventListener("click", (event) => {
    showPopover(
      event.currentTarget,
      "Create or join a workspace",
      "The production client supports hub creation and invite codes. This page keeps the flow illustrative.",
      "Preview creation",
      () => showToast("Workspace preview created locally — reload or reset to clear it.")
    );
  });

  document.querySelector("#workspace-menu").addEventListener("click", (event) => {
    showPopover(
      event.currentTarget,
      currentWorkspace().name,
      "Invite, channel, member, and workspace settings live here in the real client.",
      "Copy demo invite",
      () => showToast("Demo invite copied conceptually — no clipboard or server was used.")
    );
  });

  document.querySelector("#composer-add").addEventListener("click", (event) => {
    showPopover(
      event.currentTarget,
      "Attachments",
      "The real Kergit client includes an attachment tray and previews. Uploads are disabled on this static page.",
      "Got it",
      () => showToast("No file picker opened and no data left this page.")
    );
  });

  document.querySelectorAll(".optional-action").forEach((button) => {
    button.addEventListener("click", () => {
      showToast("This control is present for product fidelity; its data is not loaded in the static demo.");
    });
  });

  elements.channelsToggle.addEventListener("click", () => {
    const opening = !demoApp.classList.contains("channels-open");
    closePanels();
    demoApp.classList.toggle("channels-open", opening);
    elements.channelsToggle.setAttribute("aria-expanded", String(opening));
  });

  elements.membersToggle.addEventListener("click", () => {
    if (window.matchMedia("(max-width: 1120px)").matches) {
      const opening = !demoApp.classList.contains("members-open");
      closePanels();
      demoApp.classList.toggle("members-open", opening);
      elements.membersToggle.setAttribute("aria-expanded", String(opening));
      return;
    }

    const hidden = demoApp.classList.toggle("members-hidden");
    elements.membersToggle.setAttribute("aria-expanded", String(!hidden));
  });

  elements.scrim.addEventListener("click", closePanels);

  elements.reset.addEventListener("click", () => {
    window.clearTimeout(state.botTimer);
    elements.typing.hidden = true;
    state.hub = "kergit";
    state.channelByHub = {
      kergit: "genel",
      opensource: "duyurular",
      nightshift: "lobby",
    };
    state.messages = JSON.parse(JSON.stringify(initialMessages));
    state.voice = null;
    state.muted = false;
    state.deafened = false;
    state.search = "";
    elements.search.value = "";
    elements.messageInput.value = "";
    elements.camera.setAttribute("aria-pressed", "false");
    elements.screen.setAttribute("aria-pressed", "false");
    demoApp.classList.remove("members-hidden");
    closePanels();
    renderAll();
    showToast("Demo reset to its original local state.");
  });

  window.addEventListener("resize", () => {
    closePanels();
    if (!window.matchMedia("(max-width: 1120px)").matches) {
      elements.membersToggle.setAttribute("aria-expanded", String(!demoApp.classList.contains("members-hidden")));
    }
  });

  document.addEventListener("keydown", (event) => {
    if (event.key !== "Escape") return;
    closePanels();
  });

  renderAll();
})();
