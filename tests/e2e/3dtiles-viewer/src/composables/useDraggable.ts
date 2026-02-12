import { ref, type Ref } from 'vue';

export function useDraggable(
  elementRef: Ref<HTMLElement | undefined>,
  initialPosition = { x: 20, y: 20 }
) {
  const position = ref({ ...initialPosition });
  const isDragging = ref(false);
  const dragOffset = ref({ x: 0, y: 0 });

  function onMouseDown(event: MouseEvent) {
    if (!elementRef.value) return;

    isDragging.value = true;
    dragOffset.value = {
      x: event.clientX - position.value.x,
      y: event.clientY - position.value.y
    };

    document.addEventListener('mousemove', onMouseMove);
    document.addEventListener('mouseup', onMouseUp);
  }

  function onMouseMove(event: MouseEvent) {
    if (!isDragging.value) return;

    const newX = event.clientX - dragOffset.value.x;
    const newY = event.clientY - dragOffset.value.y;

    position.value = {
      x: Math.max(0, newX),
      y: Math.max(0, newY)
    };
  }

  function onMouseUp() {
    isDragging.value = false;
    document.removeEventListener('mousemove', onMouseMove);
    document.removeEventListener('mouseup', onMouseUp);
  }

  function setPosition(x: number, y: number) {
    position.value = {
      x: Math.max(0, x),
      y: Math.max(0, y)
    };
  }

  function resetPosition() {
    position.value = { ...initialPosition };
  }

  return {
    position,
    isDragging,
    onMouseDown,
    setPosition,
    resetPosition
  };
}
