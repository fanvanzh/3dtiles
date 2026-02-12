<template>
  <button
    class="toggle-button"
    :class="{
      active: isActive,
      disabled: isDisabled,
      [size]: true,
      [variant]: true
    }"
    :disabled="isDisabled"
    @click="onClick"
  >
    <span v-if="icon" class="button-icon">{{ icon }}</span>
    <span v-if="label" class="button-label">{{ label }}</span>
  </button>
</template>

<script setup lang="ts">
const props = withDefaults(
  defineProps<{
    label?: string;
    icon?: string;
    isActive?: boolean;
    isDisabled?: boolean;
    size?: 'small' | 'medium' | 'large';
    variant?: 'primary' | 'secondary' | 'danger' | 'success';
  }>(),
  {
    label: '',
    icon: '',
    isActive: false,
    isDisabled: false,
    size: 'medium',
    variant: 'secondary'
  }
);

const emit = defineEmits<{
  'click': [event: MouseEvent];
}>();

function onClick(event: MouseEvent) {
  if (props.isDisabled) return;
  emit('click', event);
}
</script>

<style scoped>
.toggle-button {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: 6px;
  border: 1px solid rgba(255, 255, 255, 0.2);
  border-radius: 6px;
  background: rgba(0, 0, 0, 0.6);
  color: #fff;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
  cursor: pointer;
  transition: all 0.2s ease;
  backdrop-filter: blur(10px);
}

.toggle-button:hover:not(.disabled) {
  background: rgba(255, 255, 255, 0.1);
  border-color: rgba(255, 255, 255, 0.3);
}

.toggle-button:active:not(.disabled) {
  transform: scale(0.98);
}

.toggle-button.active {
  background: rgba(59, 130, 246, 0.8);
  border-color: rgba(59, 130, 246, 1);
}

.toggle-button.disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.toggle-button.small {
  padding: 6px 12px;
  font-size: 11px;
}

.toggle-button.medium {
  padding: 8px 16px;
  font-size: 12px;
}

.toggle-button.large {
  padding: 10px 20px;
  font-size: 14px;
}

.toggle-button.primary.active {
  background: rgba(59, 130, 246, 0.8);
  border-color: rgba(59, 130, 246, 1);
}

.toggle-button.danger.active {
  background: rgba(239, 68, 68, 0.8);
  border-color: rgba(239, 68, 68, 1);
}

.toggle-button.success.active {
  background: rgba(34, 197, 94, 0.8);
  border-color: rgba(34, 197, 94, 1);
}

.button-icon {
  font-size: 1.2em;
  line-height: 1;
}

.button-label {
  font-weight: 500;
}
</style>
